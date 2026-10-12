// Wren Tally base: switcher client -> ESP-NOW broadcaster. Dev-board stage.
//
// What works in this version:
//   - ESP-NOW broadcast of the 4-light state every WREN_TX_PERIOD ms.
//   - Serial test mode: type "p2" (program on light 2), "v3" (preview on 3), "o" (all off),
//     "b128" (brightness), "c" (toggle cue mode), "s" (status). Enough to bench-test lights
//     with no switcher at all.
//   - vMix TCP tally client (port 8099, SUBSCRIBE TALLY) when WIFI_SSID/WIFI_PASS/VMIX_HOST are
//     set below. vMix was chosen first because its protocol is plain text and testable from this PC.
//   - Cue mode: two grounded inputs drive lights 1 and 2 (GO / STANDBY) with no switcher.
// Not yet: ATEM (UDP 9910), OBS (obs-websocket 4455), the config web page, Ethernet. See PROTOCOLS.md.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "wren_proto.h"
#include "wren_pins.h"

// ---- bench config (will move to NVS + web page) ----
static const char *WIFI_SSID = "";      // leave empty for serial-only / cue-only operation
static const char *WIFI_PASS = "";
static const char *VMIX_HOST = "";      // e.g. "192.168.1.50"
static const uint16_t VMIX_PORT = 8099;
// Light N shows vMix input INPUT_MAP[N-1] (1-based). Will be a dropdown on the web page.
static uint8_t INPUT_MAP[WREN_MAX_LIGHTS] = {1, 2, 3, 4};

static WrenPacket pkt;
static bool cueMode = false;
static WiFiClient vmix;
static uint32_t lastTxMs = 0, lastVmixTryMs = 0;
static bool switcherLive = false;
static String vmixLine;

static void sendPacket() {
  pkt.version = WREN_PROTO_VERSION;
  pkt.seq++;
  pkt.flags = (cueMode ? WREN_FLAG_CUE_MODE : 0) | (switcherLive ? WREN_FLAG_SWITCHER : 0);
  esp_now_send(WREN_BROADCAST_MAC, (const uint8_t *)&pkt, sizeof(pkt));
}

static void setAll(uint8_t s) { for (int i = 0; i < WREN_MAX_LIGHTS; i++) pkt.state[i] = s; }

static void printStatus() {
  Serial.printf("lights: %u %u %u %u  bright=%u cue=%d switcher=%d wifi=%d seq=%u\n",
                pkt.state[0], pkt.state[1], pkt.state[2], pkt.state[3], pkt.brightness,
                cueMode, switcherLive, WiFi.status() == WL_CONNECTED, pkt.seq);
}

// Serial commands for bench testing.
static void handleSerial() {
  static String line;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c != '\n') { line += c; continue; }
    line.trim();
    if (line.length()) {
      char cmd = line[0];
      int n = line.substring(1).toInt();
      if (cmd == 'p' && n >= 1 && n <= WREN_MAX_LIGHTS) { setAll(WREN_OFF); pkt.state[n - 1] = WREN_PROGRAM; }
      else if (cmd == 'v' && n >= 1 && n <= WREN_MAX_LIGHTS) { pkt.state[n - 1] = WREN_PREVIEW; }
      else if (cmd == 'o') setAll(WREN_OFF);
      else if (cmd == 'b') pkt.brightness = constrain(n, 0, 255);
      else if (cmd == 'c') cueMode = !cueMode;
      else if (cmd == 's') printStatus();
      else Serial.println("cmds: pN vN o bNNN c s");
      sendPacket();
      printStatus();
    }
    line = "";
  }
}

// vMix: "TALLY OK 0120" -> digit per input: 0 off, 1 program, 2 preview.
static void applyVmixTally(const String &digits) {
  for (int i = 0; i < WREN_MAX_LIGHTS; i++) {
    int input = INPUT_MAP[i];
    uint8_t s = WREN_OFF;
    if (input >= 1 && input <= (int)digits.length()) {
      char d = digits[input - 1];
      if (d == '1') s = WREN_PROGRAM; else if (d == '2') s = WREN_PREVIEW;
    }
    pkt.state[i] = s;
  }
  sendPacket();
}

static void pollVmix() {
  if (strlen(VMIX_HOST) == 0 || WiFi.status() != WL_CONNECTED) { switcherLive = false; return; }
  if (!vmix.connected()) {
    switcherLive = false;
    if (millis() - lastVmixTryMs < 3000) return;
    lastVmixTryMs = millis();
    Serial.printf("vMix: connecting to %s:%u\n", VMIX_HOST, VMIX_PORT);
    if (vmix.connect(VMIX_HOST, VMIX_PORT, 2000)) {
      vmix.print("SUBSCRIBE TALLY\r\n");
      vmix.print("TALLY\r\n");
    }
    return;
  }
  while (vmix.available()) {
    char c = vmix.read();
    if (c == '\r') continue;
    if (c != '\n') { vmixLine += c; continue; }
    if (vmixLine.startsWith("TALLY OK ")) {
      switcherLive = true;
      applyVmixTally(vmixLine.substring(9));
      Serial.println(vmixLine);
    }
    vmixLine = "";
  }
}

static void pollCueInputs() {
  if (!cueMode) return;
  pkt.state[0] = digitalRead(BASE_CUE_IN_1) == LOW ? WREN_PROGRAM : WREN_OFF;
  pkt.state[1] = digitalRead(BASE_CUE_IN_2) == LOW ? WREN_PROGRAM : WREN_OFF;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(BASE_STATUS_LED_PIN, OUTPUT);
  pinMode(BASE_CUE_IN_1, INPUT_PULLUP);
  pinMode(BASE_CUE_IN_2, INPUT_PULLUP);
  memset(&pkt, 0, sizeof(pkt));
  pkt.brightness = 160;

  WiFi.mode(WIFI_STA);
  if (strlen(WIFI_SSID)) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.printf("wifi: joining %s\n", WIFI_SSID);
    // NOTE: joining an AP moves us to the AP's channel; lights must be on the same one.
    // Dev-board stage: set the router to channel WREN_WIFI_CHANNEL or change WREN_WIFI_CHANNEL on both ends.
  } else {
    esp_wifi_set_channel(WREN_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  }

  if (esp_now_init() != ESP_OK) { Serial.println("esp_now_init failed"); while (true) delay(1000); }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, WREN_BROADCAST_MAC, 6);
  peer.channel = 0;  // current channel
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  Serial.printf("Wren base  mac %s\n", WiFi.macAddress().c_str());
  Serial.println("serial cmds: pN (program light N) vN (preview) o (off) bNNN (brightness) c (cue mode) s (status)");
}

void loop() {
  handleSerial();
  pollVmix();
  pollCueInputs();

  uint32_t now = millis();
  if (now - lastTxMs >= WREN_TX_PERIOD) {
    lastTxMs = now;
    sendPacket();
    digitalWrite(BASE_STATUS_LED_PIN, (pkt.seq & 1) ? HIGH : LOW);  // heartbeat
  }
  delay(5);
}
