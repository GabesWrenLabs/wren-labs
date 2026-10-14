// Wren Tally light: ESP-NOW receiver + 6 RGB LEDs. Dev-board stage, v0.6.
//
// Behaviour:
//   - Listens for WrenPacket broadcasts from the base (any base, dev-board stage).
//   - Shows its own slot: red = program, green = preview, off = idle.
//   - Cue mode (flag): red = GO (solid), green = STANDBY (solid). Same colours, the SM decides meaning.
//   - No packet for WREN_LINK_TIMEOUT -> "lost link": LED 6 blinks amber slowly, others off,
//     and the light hops through Wi-Fi channels 1..13 (WREN_SCAN_DWELL each) until it hears the
//     base again. The channel it locks on is saved in flash, so next power-up starts there.
//     This is what lets the base join any venue Wi-Fi (whose router picks the channel) with no
//     setting on the lights.
//   - Pairing (v0.6): a light that has no base saved follows ANY base (bench convenience) and adopts
//     the first base whose packet carries WREN_FLAG_PAIRING (the base's 10 s window), saving that
//     base's MAC in flash. A paired light ignores every other base, so two venues next door don't
//     cross-talk. Hold the button 3 s: the light forgets its base, blinks blue, and opens its own
//     10 s window in which it also takes a pairing packet from any base. No screens, no app.
//   - Brightness: base's value, unless the local button has been pressed (then local, cycles 4 steps).
//   - Light number comes from two jumpers (see wren_pins.h) and prints on the serial port at boot.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include "wren_proto.h"
#include "wren_pins.h"

static Adafruit_NeoPixel strip(LIGHT_NUM_LEDS, LIGHT_LED_PIN, NEO_GRB + NEO_KHZ800);
static volatile WrenPacket lastPkt;
static volatile uint32_t lastRxMs = 0;
static volatile uint32_t rxCount = 0;
static uint8_t myIndex = 0;                 // 0..3
static int localBrightness = -1;            // -1 = follow the base
static const uint8_t BRIGHT_STEPS[] = {32, 96, 160, 255};

// Channel-follow state.
static uint8_t curChannel = WREN_CHANNEL_DEFAULT;   // channel the radio is on right now
static uint8_t savedChannel = WREN_CHANNEL_DEFAULT; // channel in flash (last one that worked)
static bool scanning = false;
static uint32_t scanHopMs = 0;

// Pairing state. pairedMac is all-zero when no base is saved.
static uint8_t pairedMac[6] = {0};
static volatile bool pairPending = false;    // set in the RX callback, acted on in loop() (no NVS in callbacks)
static volatile uint8_t pendingMac[6] = {0};
static uint32_t pairWindowUntil = 0;         // light's own 10 s window after a long press (0 = closed)

static bool isPaired() {
  for (int i = 0; i < 6; i++) if (pairedMac[i]) return true;
  return false;
}

static void onRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != (int)sizeof(WrenPacket)) return;
  const WrenPacket *p = (const WrenPacket *)data;
  if (p->version != WREN_PROTO_VERSION) return;
  bool fromMyBase = isPaired() && memcmp(mac, pairedMac, 6) == 0;
  if (!fromMyBase) {
    // A stranger base. Take it only while pairing is possible: we have no base yet, or the
    // button opened our window. And only if that base is in ITS window (flag set), so a venue's
    // idle base can't be adopted by a neighbour's light by accident.
    bool mayPair = (!isPaired() || millis() < pairWindowUntil);
    if (p->flags & WREN_FLAG_PAIRING) {
      if (!mayPair) return;
      memcpy((void *)pendingMac, mac, 6);
      pairPending = true;
    } else if (isPaired()) {
      return;  // paired to someone else: ignore
    }
    // unpaired + no pairing flag: follow it anyway (dev-board / bench behaviour)
  }
  memcpy((void *)&lastPkt, p, sizeof(WrenPacket));
  lastRxMs = millis();
  rxCount++;
}

static uint8_t readLightIndex() {
  pinMode(LIGHT_ID_PIN_A, INPUT_PULLUP);
  pinMode(LIGHT_ID_PIN_B, INPUT_PULLUP);
  delay(5);
  uint8_t a = digitalRead(LIGHT_ID_PIN_A) == LOW ? 1 : 0;
  uint8_t b = digitalRead(LIGHT_ID_PIN_B) == LOW ? 2 : 0;
  return a + b;  // 0..3 -> light 1..4
}

static void fillAll(uint32_t c, uint8_t brightness) {
  strip.setBrightness(brightness);
  for (int i = 0; i < LIGHT_NUM_LEDS; i++) strip.setPixelColor(i, c);
  strip.show();
}

static uint8_t loadChannel() {
  Preferences p;
  p.begin("wrenlight", true);
  uint8_t ch = p.getUChar("chan", WREN_CHANNEL_DEFAULT);
  p.end();
  return (ch >= WREN_CHANNEL_MIN && ch <= WREN_CHANNEL_MAX) ? ch : WREN_CHANNEL_DEFAULT;
}

static void saveChannel(uint8_t ch) {
  if (ch == savedChannel) return;
  Preferences p;
  p.begin("wrenlight", false);
  p.putUChar("chan", ch);
  p.end();
  savedChannel = ch;
  Serial.printf("channel %u saved\n", ch);
}

static void loadPairedMac() {
  Preferences p;
  p.begin("wrenlight", true);
  if (p.getBytesLength("base") == 6) p.getBytes("base", pairedMac, 6);
  else memset(pairedMac, 0, 6);
  p.end();
}

static void savePairedMac(const uint8_t *mac) {
  Preferences p;
  p.begin("wrenlight", false);
  if (mac) p.putBytes("base", mac, 6); else p.remove("base");
  p.end();
  if (mac) memcpy(pairedMac, mac, 6); else memset(pairedMac, 0, 6);
}

static String macStr(const uint8_t *m) {
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(b);
}

// Returns false if the radio refused the channel (regulatory limit, e.g. 12/13 in some regions).
static bool setChannel(uint8_t ch) {
  if (esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE) != ESP_OK) return false;
  curChannel = ch;
  return true;
}

static void startScan() {
  scanning = true;
  scanHopMs = millis();
  Serial.printf("link lost on channel %u, scanning\n", curChannel);
}

static void hopChannel() {
  for (int tries = 0; tries < WREN_CHANNEL_MAX; tries++) {
    uint8_t next = curChannel + 1;
    if (next > WREN_CHANNEL_MAX) next = WREN_CHANNEL_MIN;
    if (setChannel(next)) return;
    curChannel = next;  // skip a channel the radio refuses
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  myIndex = readLightIndex();
  pinMode(LIGHT_BTN_PIN, INPUT_PULLUP);

  strip.begin();
  fillAll(strip.Color(0, 0, 255), 96);  // boot: blue for half a second
  delay(500);
  fillAll(0, 96);

  savedChannel = loadChannel();
  loadPairedMac();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (!setChannel(savedChannel)) setChannel(WREN_CHANNEL_DEFAULT);
  if (esp_now_init() != ESP_OK) {
    Serial.println("esp_now_init failed");
    while (true) { fillAll(strip.Color(128, 0, 128), 96); delay(500); fillAll(0, 96); delay(500); }
  }
  esp_now_register_recv_cb(onRecv);

  Serial.printf("Wren light %u  mac %s  channel %u (saved)  base %s\n", myIndex + 1, WiFi.macAddress().c_str(),
                curChannel, isPaired() ? macStr(pairedMac).c_str() : "none (unpaired: follows any base)");
}

// Short press: brightness step. Hold WREN_PAIR_HOLD_MS: forget the base and open a pairing window.
static void pollButton() {
  static uint32_t downSince = 0;
  static bool wasDown = false, longFired = false;
  static uint8_t step = 1;
  uint32_t now = millis();
  bool down = digitalRead(LIGHT_BTN_PIN) == LOW;
  if (down && !wasDown) { downSince = now; longFired = false; }
  if (down && !longFired && now - downSince >= WREN_PAIR_HOLD_MS) {
    longFired = true;
    savePairedMac(nullptr);
    lastRxMs = 0;  // drop the current base; show "unpaired" until a packet is accepted again
    pairWindowUntil = now + WREN_PAIR_WINDOW_MS;
    Serial.println("button held: base forgotten, pairing window open 10 s");
  }
  if (!down && wasDown && !longFired && now - downSince > 30) {  // short press released (debounced)
    step = (step + 1) % 4;
    localBrightness = BRIGHT_STEPS[step];
    Serial.printf("brightness -> %d (local)\n", localBrightness);
  }
  wasDown = down;
}

void loop() {
  pollButton();

  uint32_t now = millis();

  if (pairPending) {
    uint8_t m[6];
    memcpy(m, (const void *)pendingMac, 6);
    pairPending = false;
    if (memcmp(m, pairedMac, 6) != 0) {
      savePairedMac(m);
      pairWindowUntil = 0;
      Serial.printf("paired to base %s (saved)\n", macStr(m).c_str());
      fillAll(strip.Color(0, 0, 255), 160); delay(300);  // one blue flash = paired
    }
  }

  if (pairWindowUntil && now < pairWindowUntil) {
    // Our own window: blink blue fast until a base in its window is heard (or 10 s pass).
    strip.setBrightness(96);
    fillAll((now / 150) % 2 == 0 ? strip.Color(0, 0, 255) : 0, 96);
    delay(20);
    return;
  }
  pairWindowUntil = 0;

  bool linked = (lastRxMs != 0) && (now - lastRxMs < WREN_LINK_TIMEOUT);

  if (!linked) {
    // Scan: wait one timeout on the saved channel first (the base may just be rebooting),
    // then hop every WREN_SCAN_DWELL until a packet arrives.
    if (!scanning && (lastRxMs != 0 || now > WREN_LINK_TIMEOUT)) startScan();
    if (scanning && now - scanHopMs >= WREN_SCAN_DWELL) {
      scanHopMs = now;
      hopChannel();
    }
    bool blinkOn = (now / 500) % 2 == 0;
    strip.setBrightness(48);
    strip.clear();
    strip.setPixelColor(LIGHT_NUM_LEDS - 1, blinkOn ? strip.Color(255, 120, 0) : 0);  // amber, slow
    strip.show();
    delay(20);
    return;
  }

  if (scanning) {
    scanning = false;
    Serial.printf("base found on channel %u\n", curChannel);
    saveChannel(curChannel);
  } else if (curChannel != savedChannel) {
    saveChannel(curChannel);  // first link after boot on a channel that wasn't saved yet
  }

  WrenPacket p;
  memcpy(&p, (const void *)&lastPkt, sizeof(p));
  uint8_t s = p.state[myIndex];
  uint32_t c = 0;
  if (s & WREN_PROGRAM) c = strip.Color(255, 0, 0);        // program wins over preview
  else if (s & WREN_PREVIEW) c = strip.Color(0, 255, 0);

  fillAll(c, localBrightness >= 0 ? localBrightness : p.brightness);

  static uint32_t lastLog = 0;
  if (now - lastLog > 5000) {
    lastLog = now;
    Serial.printf("rx=%lu seq=%u mine=%u flags=0x%02x bright=%u ch=%u (base says %u) base=%s\n", (unsigned long)rxCount,
                  p.seq, s, p.flags, p.brightness, curChannel, p.channel,
                  isPaired() ? macStr(pairedMac).c_str() : "any");
  }
  delay(10);
}
