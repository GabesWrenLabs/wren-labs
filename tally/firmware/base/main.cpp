// Wren Tally base: switcher client -> ESP-NOW broadcaster. Dev-board stage, v0.6.
//
// What works in this version:
//   - Any venue Wi-Fi channel: the base sits wherever the router puts it, lights scan and follow
//     (wren_proto.h "Channel-follow"); the packet and the status line both report the channel.
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
//   - Pairing (v0.6): BOOT button, serial 'a' or the web "Pair lights" button opens a 10 s window;
//     packets carry WREN_FLAG_PAIRING and lights that may pair save this base's MAC (wren_proto.h).
// Not yet: Ethernet, OTA partition table. See PROTOCOLS.md.
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
static uint32_t pairUntilMs = 0;  // > millis() while the pairing window is open

static bool pairingOpen() { return pairUntilMs && (int32_t)(pairUntilMs - millis()) > 0; }
static void openPairing() {
  pairUntilMs = millis() + WREN_PAIR_WINDOW_MS;
  Serial.printf("pairing window open for %u s (base mac %s)\n", WREN_PAIR_WINDOW_MS / 1000, WiFi.macAddress().c_str());
}

static uint8_t currentChannel() {
  uint8_t primary = 0;
  wifi_second_chan_t second;
  if (esp_wifi_get_channel(&primary, &second) != ESP_OK) return 0;
  return primary;
}

static void sendPacket() {
  pkt.version = WREN_PROTO_VERSION;
  pkt.seq++;
  pkt.flags = (cueMode ? WREN_FLAG_CUE_MODE : 0) | (switcherLive ? WREN_FLAG_SWITCHER : 0) |
              (pairingOpen() ? WREN_FLAG_PAIRING : 0);
  pkt.channel = currentChannel();  // informational; lights find us by scanning (wren_proto.h)
  esp_now_send(WREN_BROADCAST_MAC, (const uint8_t *)&pkt, sizeof(pkt));
}

static void setAll(uint8_t s) { for (int i = 0; i < WREN_MAX_LIGHTS; i++) pkt.state[i] = s; }

static String statusText() {
  char buf[200];
  char pairing[32] = "";
  if (pairingOpen()) snprintf(pairing, sizeof(pairing), "  PAIRING %lu s left", (unsigned long)((pairUntilMs - millis()) / 1000));
  snprintf(buf, sizeof(buf), "lights: %u %u %u %u  bright=%u cue=%d\nswitcher: %s %s:%u %s\nwifi: %s%s  channel=%u  seq=%u%s",
           pkt.state[0], pkt.state[1], pkt.state[2], pkt.state[3], pkt.brightness, cueMode,
           cfg.typeName(), cfg.host.c_str(), cfg.effectivePort(), switcherLive ? "CONNECTED" : "not connected",
           WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "not joined",
           web.apUp ? " (setup AP up)" : "", currentChannel(), pkt.seq, pairing);
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
      else if (cmd == 'a') openPairing();
      else if (cmd == 'f') { Serial.println("factory reset"); WrenConfig::factoryReset(); delay(200); ESP.restart(); }
      else Serial.println("cmds: pN vN o bNNN c s a f");
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

// BOOT button: a press (released within 3 s) opens the pairing window. Holding it longer does nothing
// yet (reserved for factory reset on the real board).
static void pollPairButton() {
  static bool wasDown = false;
  static uint32_t downSince = 0;
  uint32_t now = millis();
  bool down = digitalRead(BASE_PAIR_BTN_PIN) == LOW;
  if (down && !wasDown) downSince = now;
  if (!down && wasDown && now - downSince > 30 && now - downSince < 3000) openPairing();
  wasDown = down;
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
  pinMode(BASE_PAIR_BTN_PIN, INPUT_PULLUP);
  memset(&pkt, 0, sizeof(pkt));

  cfg.load();
  pkt.brightness = cfg.brightness;
  Serial.printf("config: wifi=\"%s\" switcher=%s host=\"%s\" port=%u map=%u,%u,%u,%u bright=%u\n",
                cfg.ssid.c_str(), cfg.typeName(), cfg.host.c_str(), cfg.effectivePort(),
                cfg.inputMap[0], cfg.inputMap[1], cfg.inputMap[2], cfg.inputMap[3], cfg.brightness);

  // Brings up STA (if a venue Wi-Fi is saved) and the setup AP, then the web page.
  // Joining a venue AP moves the radio to that AP's channel. That's fine: lights scan for us
  // (see wren_proto.h "Channel-follow") and the packet carries the channel for the serial log.
  web.onPair = openPairing;
  web.begin(&cfg, statusText, WREN_WIFI_CHANNEL);

  if (esp_now_init() != ESP_OK) { Serial.println("esp_now_init failed"); while (true) delay(1000); }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, WREN_BROADCAST_MAC, 6);
  peer.channel = 0;  // current channel
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  Serial.printf("Wren base  mac %s\n", WiFi.macAddress().c_str());
  Serial.println("serial cmds: pN (program light N) vN (preview) o (off) bNNN (brightness) c (cue mode) s (status) a (pair 10 s) f (factory reset)");
}

void loop() {
  handleSerial();
  web.loop();
  pollSwitcher();
  pollPairButton();
  pollCueInputs();

  uint32_t now = millis();
  if (now - lastTxMs >= WREN_TX_PERIOD) {
    lastTxMs = now;
    sendPacket();
    digitalWrite(BASE_STATUS_LED_PIN, (pkt.seq & 1) ? HIGH : LOW);  // heartbeat
  }
  delay(5);
}
