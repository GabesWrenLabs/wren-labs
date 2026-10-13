// Wren Tally base: switcher client -> ESP-NOW broadcaster. Dev-board stage, v0.4.
//
// What works in this version:
//   - Settings live in NVS (config.h) and are edited on a web page (web_config.h): venue Wi-Fi,
//     switcher type + IP, which input / OBS scene each light follows, brightness. No constants to
//     edit, no reflash to move the base to a new venue.
//   - With no Wi-Fi saved (or the venue Wi-Fi down), the base makes its own "WrenTally-XXXX"
//     network (password wrentally) and the page is at http://192.168.4.1/
//   - ESP-NOW broadcast of the 4-light state every WREN_TX_PERIOD ms.
//   - Serial test mode: "p2" (program on light 2), "v3" (preview on 3), "o" (all off), "b128"
//     (brightness), "c" (toggle cue mode), "s" (status), "f" (factory reset + reboot).
//   - vMix TCP tally client (port 8099, SUBSCRIBE TALLY).
//   - ATEM UDP tally client (port 9910, TlIn). Untested on a real switcher; see atem_client.h.
//   - OBS Studio client (obs-websocket v5, port 4455). Lights follow scene names. Untested; obs_client.h.
//   - Cue mode: two grounded inputs drive lights 1 and 2 (GO / STANDBY) with no switcher.
// Not yet: Ethernet, light pairing by MAC. See PROTOCOLS.md.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "wren_proto.h"
#include "wren_pins.h"
#include "config.h"
#include "web_config.h"
#include "atem_client.h"
#include "obs_client.h"

static WrenConfig cfg;
static WebConfig web;
static WrenPacket pkt;
static bool cueMode = false;
static WiFiClient vmix;
static AtemClient atem;
static ObsClient obs;
static bool atemStarted = false, obsStarted = false;
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

static String statusText() {
  char buf[200];
  snprintf(buf, sizeof(buf), "lights: %u %u %u %u  bright=%u cue=%d\nswitcher: %s %s:%u %s\nwifi: %s%s  seq=%u",
           pkt.state[0], pkt.state[1], pkt.state[2], pkt.state[3], pkt.brightness, cueMode,
           cfg.typeName(), cfg.host.c_str(), cfg.effectivePort(), switcherLive ? "CONNECTED" : "not connected",
           WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "not joined",
           web.apUp ? " (setup AP up)" : "", pkt.seq);
  return String(buf);
}

static void printStatus() { Serial.println(statusText()); }

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
      else if (cmd == 'f') { Serial.println("factory reset"); WrenConfig::factoryReset(); delay(200); ESP.restart(); }
      else Serial.println("cmds: pN vN o bNNN c s f");
      sendPacket();
      printStatus();
    }
    line = "";
  }
}

// vMix: "TALLY OK 0120" -> digit per input: 0 off, 1 program, 2 preview.
static void applyVmixTally(const String &digits) {
  for (int i = 0; i < WREN_MAX_LIGHTS; i++) {
    int input = cfg.inputMap[i];
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
  if (WiFi.status() != WL_CONNECTED) { switcherLive = false; return; }
  if (!vmix.connected()) {
    switcherLive = false;
    if (millis() - lastVmixTryMs < 3000) return;
    lastVmixTryMs = millis();
    Serial.printf("vMix: connecting to %s:%u\n", cfg.host.c_str(), cfg.effectivePort());
    if (vmix.connect(cfg.host.c_str(), cfg.effectivePort(), 2000)) {
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

// ATEM: TlIn gives one byte per input (bit0 program, bit1 preview). Lights follow inputMap.
static void pollAtem() {
  if (WiFi.status() != WL_CONNECTED) { switcherLive = false; return; }
  if (!atemStarted) { atem.begin(cfg.host.c_str(), cfg.effectivePort()); atemStarted = true; }
  bool changed = atem.poll();
  switcherLive = atem.connected;
  if (changed && atem.connected) {
    for (int i = 0; i < WREN_MAX_LIGHTS; i++) pkt.state[i] = atem.stateForInput(cfg.inputMap[i]);
    sendPacket();
    Serial.printf("ATEM tally: %u %u %u %u\n", pkt.state[0], pkt.state[1], pkt.state[2], pkt.state[3]);
  }
}

// OBS: lights follow scene names.
static void pollObs() {
  if (WiFi.status() != WL_CONNECTED) { switcherLive = false; return; }
  if (!obsStarted) { obs.begin(cfg.host.c_str(), cfg.effectivePort(), cfg.obsPass.c_str()); obsStarted = true; }
  bool changed = obs.poll();
  switcherLive = obs.connected;
  if (changed && obs.connected) {
    for (int i = 0; i < WREN_MAX_LIGHTS; i++) pkt.state[i] = obs.stateForScene(cfg.sceneMap[i].c_str());
    sendPacket();
    Serial.printf("OBS tally: %u %u %u %u  (program \"%s\", preview \"%s\")\n", pkt.state[0], pkt.state[1],
                  pkt.state[2], pkt.state[3], obs.programScene.c_str(), obs.previewScene.c_str());
  }
}

static void pollSwitcher() {
  if (cfg.host.length() == 0) { switcherLive = false; return; }
  switch (cfg.type) {
    case SW_VMIX: pollVmix(); break;
    case SW_ATEM: pollAtem(); break;
    case SW_OBS:  pollObs();  break;
    default:      switcherLive = false; break;
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

  cfg.load();
  pkt.brightness = cfg.brightness;
  Serial.printf("config: wifi=\"%s\" switcher=%s host=\"%s\" port=%u map=%u,%u,%u,%u bright=%u\n",
                cfg.ssid.c_str(), cfg.typeName(), cfg.host.c_str(), cfg.effectivePort(),
                cfg.inputMap[0], cfg.inputMap[1], cfg.inputMap[2], cfg.inputMap[3], cfg.brightness);

  // Brings up STA (if a venue Wi-Fi is saved) and the setup AP, then the web page.
  // NOTE: joining a venue AP moves the radio to that AP's channel; lights must be on the same one.
  // Dev-board stage: set the router to channel WREN_WIFI_CHANNEL or change WREN_WIFI_CHANNEL on both ends.
  web.begin(&cfg, statusText, WREN_WIFI_CHANNEL);

  if (esp_now_init() != ESP_OK) { Serial.println("esp_now_init failed"); while (true) delay(1000); }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, WREN_BROADCAST_MAC, 6);
  peer.channel = 0;  // current channel
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  Serial.printf("Wren base  mac %s\n", WiFi.macAddress().c_str());
  Serial.println("serial cmds: pN (program light N) vN (preview) o (off) bNNN (brightness) c (cue mode) s (status) f (factory reset)");
}

void loop() {
  handleSerial();
  web.loop();
  pollSwitcher();
  pollCueInputs();

  uint32_t now = millis();
  if (now - lastTxMs >= WREN_TX_PERIOD) {
    lastTxMs = now;
    sendPacket();
    digitalWrite(BASE_STATUS_LED_PIN, (pkt.seq & 1) ? HIGH : LOW);  // heartbeat
  }
  delay(5);
}
