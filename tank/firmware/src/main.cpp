// Wren Tank Alert: LoRa range-test firmware for two Heltec WiFi LoRa 32 V3 boards.
//
// Same image on both boards. At power-up:
//   PRG/BOOT button HELD   -> HOUSE: sits at the window, answers every ping, shows link quality.
//   PRG/BOOT button free   -> TRUCK: pings every 2 s, shows whether the house answered and how loud.
//
// Packet (plain text so a serial monitor can read it):
//   truck -> house   "P,<seq>,<truck_batt_mV>"
//   house -> truck   "A,<seq>,<house_rssi>,<house_snr>"
// Both boards print one CSV line per packet on USB serial (115200) for logging from a laptop.
//
// Radio: 915 MHz, BW 125 kHz, SF10, CR 4/5, 22 dBm, private sync word. Changing SF is one line below.

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <U8g2lib.h>

// ---- Heltec WiFi LoRa 32 V3 pins ----
static const int PIN_LORA_NSS  = 8;
static const int PIN_LORA_SCK  = 9;
static const int PIN_LORA_MOSI = 10;
static const int PIN_LORA_MISO = 11;
static const int PIN_LORA_RST  = 12;
static const int PIN_LORA_BUSY = 13;
static const int PIN_LORA_DIO1 = 14;
static const int PIN_OLED_SDA  = 17;
static const int PIN_OLED_SCL  = 18;
static const int PIN_OLED_RST  = 21;
static const int PIN_VEXT      = 36;   // LOW = power to the OLED
static const int PIN_LED       = 35;   // white LED next to the USB port
static const int PIN_BUTTON    = 0;    // PRG / BOOT, LOW when pressed
static const int PIN_ADC_CTRL  = 37;   // LOW = enable the battery divider
static const int PIN_VBAT      = 1;    // battery divider, 390k/100k

// ---- Radio settings ----
static const float    RF_FREQ_MHZ  = 915.0;
static const float    RF_BW_KHZ    = 125.0;
static const uint8_t  RF_SF        = 10;      // 7..12; 10 is the plan's starting point
static const uint8_t  RF_CR        = 5;       // 4/5
static const uint8_t  RF_SYNC      = 0x34;    // private network sync word (RadioLib default 0x12 is fine too)
static const int8_t   RF_POWER_DBM = 22;
static const uint16_t RF_PREAMBLE  = 8;
static const float    RF_TCXO_V    = 1.8;     // Heltec V3 feeds the SX1262 TCXO from DIO3 at 1.8 V

static const uint32_t PING_PERIOD_MS = 2000;
static const uint32_t ACK_WAIT_MS    = 1200;  // SF10/125k: a 20-byte packet is ~330 ms each way
static const uint32_t HOUSE_SILENT_MS = 10000; // house shows "NO SIGNAL" after this long without a ping

SX1262 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, PIN_OLED_RST, PIN_OLED_SCL, PIN_OLED_SDA);

enum Role { TRUCK, HOUSE };
static Role role = TRUCK;

static volatile bool rxFlag = false;
static void IRAM_ATTR onRadioIrq() { rxFlag = true; }

// ---- stats ----
static uint32_t seq = 0;           // truck: pings sent; house: last seq seen
static uint32_t acked = 0;         // truck: pings answered
static uint32_t lost = 0;          // truck: pings with no answer; house: gaps in seq
static uint32_t received = 0;      // house: pings received
static float    lastRssi = 0, lastSnr = 0;      // what THIS board measured on the last packet
static float    farRssi = 0, farSnr = 0;        // truck only: what the house measured
static uint32_t lastPacketMs = 0;
static bool     everHeard = false;

static int batteryMv() {
  pinMode(PIN_ADC_CTRL, OUTPUT);
  digitalWrite(PIN_ADC_CTRL, LOW);
  delay(5);
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(PIN_VBAT);
  digitalWrite(PIN_ADC_CTRL, HIGH);
  // divider 390k over 100k: Vbat = Vadc * 4.9
  return (int)((sum / 8) * 4.9f);
}

static void draw(const char* status) {
  char line[32];
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  snprintf(line, sizeof line, "%s  SF%u  %s", role == HOUSE ? "HOUSE" : "TRUCK", RF_SF, WREN_RANGE_VERSION);
  oled.drawStr(0, 10, line);
  oled.drawHLine(0, 12, 128);

  oled.setFont(u8g2_font_7x14B_tf);
  oled.drawStr(0, 27, status);

  oled.setFont(u8g2_font_6x12_tf);
  if (role == TRUCK) {
    snprintf(line, sizeof line, "sent %lu ok %lu lost %lu", (unsigned long)seq, (unsigned long)acked, (unsigned long)lost);
    oled.drawStr(0, 40, line);
    snprintf(line, sizeof line, "here %4.0f dBm %5.1f", lastRssi, lastSnr);
    oled.drawStr(0, 52, line);
    snprintf(line, sizeof line, "house %4.0f dBm %5.1f", farRssi, farSnr);
    oled.drawStr(0, 63, line);
  } else {
    snprintf(line, sizeof line, "got %lu lost %lu seq %lu", (unsigned long)received, (unsigned long)lost, (unsigned long)seq);
    oled.drawStr(0, 40, line);
    snprintf(line, sizeof line, "rssi %4.0f dBm snr %5.1f", lastRssi, lastSnr);
    oled.drawStr(0, 52, line);
    uint32_t ago = everHeard ? (millis() - lastPacketMs) / 1000 : 0;
    snprintf(line, sizeof line, "last %lus ago", (unsigned long)ago);
    oled.drawStr(0, 63, line);
  }
  oled.sendBuffer();
}

static void halt(const char* why, int code) {
  char line[32];
  snprintf(line, sizeof line, "%s %d", why, code);
  Serial.println(line);
  draw(line);
  while (true) { digitalWrite(PIN_LED, !digitalRead(PIN_LED)); delay(150); }
}

static void startListening() {
  rxFlag = false;
  radio.startReceive();
}

void setup() {
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_VEXT, OUTPUT);
  digitalWrite(PIN_VEXT, LOW);     // OLED on
  delay(100);

  Serial.begin(115200);
  delay(300);                      // the button is sampled here: hold PRG while plugging in = HOUSE
  role = (digitalRead(PIN_BUTTON) == LOW) ? HOUSE : TRUCK;

  oled.begin();
  draw("booting");

  SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);
  int st = radio.begin(RF_FREQ_MHZ, RF_BW_KHZ, RF_SF, RF_CR, RF_SYNC, RF_POWER_DBM, RF_PREAMBLE, RF_TCXO_V, false);
  if (st != RADIOLIB_ERR_NONE) halt("radio err", st);
  radio.setDio2AsRfSwitch(true);   // Heltec V3 switches the antenna with DIO2
  radio.setCRC(true);
  radio.setPacketReceivedAction(onRadioIrq);

  Serial.printf("# wren-range %s role=%s f=%.1fMHz bw=%.0fkHz sf=%u cr=4/%u pwr=%ddBm batt=%dmV\n",
                WREN_RANGE_VERSION, role == HOUSE ? "HOUSE" : "TRUCK", RF_FREQ_MHZ, RF_BW_KHZ, RF_SF, RF_CR,
                RF_POWER_DBM, batteryMv());
  Serial.println(role == HOUSE ? "# ms,seq,rssi,snr,lost" : "# ms,seq,acked,here_rssi,here_snr,house_rssi,house_snr,lost");

  if (role == HOUSE) { startListening(); draw("waiting"); }
  else               { draw("pinging"); }
}

static void loopTruck() {
  uint32_t t0 = millis();
  seq++;
  char msg[40];
  snprintf(msg, sizeof msg, "P,%lu,%d", (unsigned long)seq, batteryMv());
  digitalWrite(PIN_LED, HIGH);
  int st = radio.transmit(msg);
  digitalWrite(PIN_LED, LOW);
  if (st != RADIOLIB_ERR_NONE) Serial.printf("# tx err %d\n", st);

  startListening();
  bool got = false;
  while (millis() - t0 < ACK_WAIT_MS + 400) {
    if (rxFlag) {
      rxFlag = false;
      String s;
      int rs = radio.readData(s);
      if (rs == RADIOLIB_ERR_NONE) {
        unsigned long aseq = 0; float hr = 0, hs = 0;
        if (sscanf(s.c_str(), "A,%lu,%f,%f", &aseq, &hr, &hs) == 3 && aseq == seq) {
          got = true;
          lastRssi = radio.getRSSI(); lastSnr = radio.getSNR();
          farRssi = hr; farSnr = hs;
          break;
        }
      }
      startListening();
    }
    delay(5);
  }
  radio.standby();
  if (got) { acked++; lastPacketMs = millis(); everHeard = true; }
  else     { lost++; }

  Serial.printf("%lu,%lu,%d,%.0f,%.1f,%.0f,%.1f,%lu\n", (unsigned long)millis(), (unsigned long)seq, got ? 1 : 0,
                got ? lastRssi : 0.0f, got ? lastSnr : 0.0f, got ? farRssi : 0.0f, got ? farSnr : 0.0f,
                (unsigned long)lost);

  char status[24];
  if (got)               snprintf(status, sizeof status, "LINK OK");
  else if (!everHeard)   snprintf(status, sizeof status, "no answer yet");
  else                   snprintf(status, sizeof status, "LOST %lus", (unsigned long)((millis() - lastPacketMs) / 1000));
  draw(status);

  while (millis() - t0 < PING_PERIOD_MS) delay(5);
}

static void loopHouse() {
  if (rxFlag) {
    rxFlag = false;
    String s;
    int rs = radio.readData(s);
    if (rs == RADIOLIB_ERR_NONE) {
      unsigned long pseq = 0; int tb = 0;
      if (sscanf(s.c_str(), "P,%lu,%d", &pseq, &tb) >= 1) {
        lastRssi = radio.getRSSI(); lastSnr = radio.getSNR();
        if (everHeard && pseq > seq + 1) lost += (pseq - seq - 1);
        seq = pseq; received++; lastPacketMs = millis(); everHeard = true;
        digitalWrite(PIN_LED, HIGH);
        char ack[40];
        snprintf(ack, sizeof ack, "A,%lu,%.0f,%.1f", pseq, lastRssi, lastSnr);
        radio.transmit(ack);
        digitalWrite(PIN_LED, LOW);
        Serial.printf("%lu,%lu,%.0f,%.1f,%lu\n", (unsigned long)millis(), pseq, lastRssi, lastSnr, (unsigned long)lost);
      }
    } else if (rs == RADIOLIB_ERR_CRC_MISMATCH) {
      Serial.println("# crc");
    }
    startListening();
  }

  static uint32_t lastDraw = 0;
  if (millis() - lastDraw > 500) {
    lastDraw = millis();
    char status[24];
    if (!everHeard)                                   snprintf(status, sizeof status, "waiting");
    else if (millis() - lastPacketMs > HOUSE_SILENT_MS) snprintf(status, sizeof status, "NO SIGNAL");
    else                                              snprintf(status, sizeof status, "LINK OK");
    draw(status);
  }
  delay(2);
}

void loop() {
  if (role == HOUSE) loopHouse();
  else               loopTruck();
}
