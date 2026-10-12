// Wren Tally light: ESP-NOW receiver + 6 RGB LEDs. Dev-board stage.
//
// Behaviour:
//   - Listens for WrenPacket broadcasts from the base (any base, dev-board stage).
//   - Shows its own slot: red = program, green = preview, off = idle.
//   - Cue mode (flag): red = GO (solid), green = STANDBY (solid). Same colours, the SM decides meaning.
//   - No packet for WREN_LINK_TIMEOUT -> "lost link": LED 6 blinks amber slowly, others off.
//   - Brightness: base's value, unless the local button has been pressed (then local, cycles 4 steps).
//   - Light number comes from two jumpers (see wren_pins.h) and prints on the serial port at boot.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
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

static void onRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != (int)sizeof(WrenPacket)) return;
  const WrenPacket *p = (const WrenPacket *)data;
  if (p->version != WREN_PROTO_VERSION) return;
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

void setup() {
  Serial.begin(115200);
  delay(300);
  myIndex = readLightIndex();
  pinMode(LIGHT_BTN_PIN, INPUT_PULLUP);

  strip.begin();
  fillAll(strip.Color(0, 0, 255), 96);  // boot: blue for half a second
  delay(500);
  fillAll(0, 96);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(WREN_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("esp_now_init failed");
    while (true) { fillAll(strip.Color(128, 0, 128), 96); delay(500); fillAll(0, 96); delay(500); }
  }
  esp_now_register_recv_cb(onRecv);

  Serial.printf("Wren light %u  mac %s  channel %u\n", myIndex + 1, WiFi.macAddress().c_str(), WREN_WIFI_CHANNEL);
}

static void pollButton() {
  static uint32_t lastPress = 0;
  static bool wasDown = false;
  static uint8_t step = 1;
  bool down = digitalRead(LIGHT_BTN_PIN) == LOW;
  if (down && !wasDown && millis() - lastPress > 250) {
    lastPress = millis();
    step = (step + 1) % 4;
    localBrightness = BRIGHT_STEPS[step];
    Serial.printf("brightness -> %d (local)\n", localBrightness);
  }
  wasDown = down;
}

void loop() {
  pollButton();

  uint32_t now = millis();
  bool linked = (lastRxMs != 0) && (now - lastRxMs < WREN_LINK_TIMEOUT);

  if (!linked) {
    bool blinkOn = (now / 500) % 2 == 0;
    strip.setBrightness(48);
    strip.clear();
    strip.setPixelColor(LIGHT_NUM_LEDS - 1, blinkOn ? strip.Color(255, 120, 0) : 0);  // amber, slow
    strip.show();
    delay(20);
    return;
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
    Serial.printf("rx=%lu seq=%u mine=%u flags=0x%02x bright=%u\n", (unsigned long)rxCount, p.seq, s, p.flags, p.brightness);
  }
  delay(10);
}
