// esp32_c3_tof_main.cpp
//
// Stage 2: boot confirmation + cm.mm live readout.
// Boots, confirms display and sensor are both alive by showing "VV"
// for 5s, then switches to continuous live distance readings.
// Screen shows cm.mm (compact, readable); Serial logs raw mm
// (precise, for future ESP-NOW use). Also prints this device's MAC
// address at boot - needed later for ESP-NOW pairing.
//
// IMPORTANT: the OLED and the ToF sensor are on TWO SEPARATE I2C
// buses, on different pins - not sharing a bus. Sharing GPIO5/6
// between u8g2's software I2C (bit-banging) and the sensor's
// hardware-I2C comms caused a persistent conflict - the display
// library appeared to repeatedly grab raw GPIO control of the shared
// pins on every redraw, cutting the sensor off. Confirmed via
// extensive testing: bus recovery, pull-ups, and 5V power all failed
// identically; only physically separating the pins resolved it.
//
// OLED bus: GPIO5 (SDA) / GPIO6 (SCL), address 0x3C - u8g2 software
// I2C, does NOT use the Wire library or the hardware I2C peripheral
// at all - pure GPIO bit-banging.
// ToF bus:  GPIO8 (SDA) / GPIO9 (SCL), address 0x29 - the chip's one
// hardware I2C controller (ESP32-C3 only has one), used exclusively
// for the sensor since u8g2 never touches it.
#define OLED_SDA 5
#define OLED_SCL 6
#define TOF_SDA 8
#define TOF_SCL 9

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include "../../shared/EspNowProtocol.h"
#include "../../shared/EspNowTiming.h"

// Same bridge MAC the other esp32_2026 devices announce to.
static uint8_t bridgeMac[6] = {0xCC, 0xDB, 0xA7, 0x69, 0x97, 0xDC};

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ OLED_SCL, /* data=*/ OLED_SDA, /* reset=*/ U8X8_PIN_NONE);
Adafruit_VL53L0X tof = Adafruit_VL53L0X();
bool tofReady = false;

// This panel is physically 72x40 pixels, sitting inside a controller
// that thinks it's addressing 128x64 - without this offset, anything
// drawn near (0,0) renders off the visible area entirely.
const int PANEL_W = 72;
const int PANEL_H = 40;
const int X_OFFSET = 30;  // (132 - PANEL_W) / 2
const int Y_OFFSET = 12;  // (64 - PANEL_H) / 2

// Native USB needs an actual connected listener on the other end. If
// the Serial Monitor is closed but the code keeps printing, the
// output buffer fills up and further Serial.print/println calls
// BLOCK waiting for room that will never free - stalling all of
// loop() (sensor reading, display update, everything) even though
// neither actually depends on a PC being connected.
//
// The standard "if (Serial)" connection check turned out NOT to
// reliably detect a closed monitor on this chip/core - so instead of
// inferring connection state, check the actual thing that causes the
// freeze directly: whether there's room in the output buffer to
// write without blocking. If there isn't, skip the print entirely
// rather than wait for space that may never free up.
static void dbgPrintln(const String &s) {
  if (Serial.availableForWrite() >= (int)(s.length() + 2)) {
    Serial.println(s);
  }
}
static void dbgPrint(const String &s) {
  if (Serial.availableForWrite() >= (int)s.length()) {
    Serial.print(s);
  }
}

static void showCentered(const char *text) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso22_tr);
  int w = u8g2.getStrWidth(text);
  int x = X_OFFSET + (PANEL_W - w) / 2;
  int y = Y_OFFSET + PANEL_H / 2 + 14;  // rough vertical centering for this font's baseline
  u8g2.drawStr(x, y, text);
  u8g2.sendBuffer();
}

// Returns true once every intervalMs, without ever blocking. Pass
// each call site its own `lastTime` variable (by reference) so
// independent timers don't interfere with each other.
static bool every(unsigned long intervalMs, unsigned long &lastTime) {
  unsigned long now = millis();
  if (now - lastTime >= intervalMs) {
    lastTime = now;
    return true;
  }
  return false;
}

// Lightweight "is this address responding right now" check on the
// ToF's own bus (Wire) - a single beginTransmission/endTransmission,
// not a bus reset. Safe to call every loop pass.
static bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

const uint8_t TOF_ADDR = 0x29;
String bootStatus;  // captured once in setup(), reprinted periodically so it's never missed

bool espNowReady = false;
uint16_t lastReportedMm = 0;
bool lastReportedValid = false;
const uint16_t REPORT_CHANGE_THRESHOLD_MM = 5;  // send a STATE_CHANGE only past this much drift,
                                                  // not on every single 200ms sample

static bool initEspNow() {
  if (esp_now_init() != ESP_OK) {
    dbgPrintln("ESP-NOW init failed");
    return false;
  }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, bridgeMac, 6);
  peer.channel = 0;  // use whatever channel this device's WiFi is already on
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    dbgPrintln("Failed to add ESP-NOW peer (bridge)");
    return false;
  }
  return true;
}

static void sendDistanceMessage(uint16_t distanceMm, bool valid, uint8_t reason) {
  if (!espNowReady) return;
  EspNowMessage msg = {};
  msg.type = MSG_DISTANCE_SENSOR;
  msg.payload.distanceSensor.distanceMm = distanceMm;
  msg.payload.distanceSensor.valid = valid ? 1 : 0;
  msg.payload.distanceSensor.reason = reason;
  esp_now_send(bridgeMac, (uint8_t *)&msg, sizeof(msg));
}

void setup() {
  Serial.begin(115200);
  delay(3000);  // give native-USB CDC a moment to actually connect

  dbgPrintln("=== BOOT ===");

  // Needed to register this device by MAC on the bridge side later
  // for ESP-NOW. Doesn't join any network - just reads the chip's
  // own factory MAC.
  WiFi.mode(WIFI_STA);
  String macAddr = WiFi.macAddress();
  dbgPrint("MAC Address: ");
  dbgPrintln(macAddr);

  espNowReady = initEspNow();
  dbgPrintln(espNowReady ? "ESP-NOW ready" : "ESP-NOW init FAILED");

  // OLED: u8g2 software I2C manages GPIO5/6 directly - no Wire.begin()
  // needed or wanted here, to keep these pins exclusively under u8g2's
  // control with zero interference from the Wire library.
  bool oledReady = u8g2.begin();
  dbgPrintln(oledReady ? "u8g2.begin() succeeded" : "u8g2.begin() FAILED");
  u8g2.setContrast(255);

  if (oledReady) {
    showCentered("Hi");  // unconditional display-alive check, independent of sensor status
    delay(2000);
  }

  // ToF: separate pins, the chip's one hardware I2C peripheral -
  // u8g2 never touches this bus at all.
  Wire.begin(TOF_SDA, TOF_SCL);

  tofReady = false;
  if (i2cPresent(TOF_ADDR)) {
    tofReady = tof.begin(TOF_ADDR, false, &Wire);
    dbgPrintln(tofReady ? "VL53L0X ready" : "VL53L0X present but init failed");
  } else {
    dbgPrintln("VL53L0X not detected at boot - loop() will keep checking");
  }

  bootStatus = String("MAC: ") + macAddr +
               " | OLED: " + (oledReady ? "OK" : "FAILED") +
               " | ToF@boot: " + (tofReady ? "OK" : "not found");

  sendDistanceMessage(0, false, REASON_BOOT);

  if (oledReady && tofReady) {
    // Both confirmed alive - hold this for 5s before live readings
    // start. This is a one-time boot-phase delay, not inside the
    // ongoing loop, so it doesn't block anything that matters.
    dbgPrintln("Display + sensor both alive - showing VV confirmation");
    showCentered("VV");
    delay(5000);
  } else if (!tofReady) {
    showCentered("N/C");
  }

  dbgPrintln("=== setup() complete, entering loop() ===");
}

unsigned long lastHeartbeat = 0;
unsigned long lastMeasurement = 0;
unsigned long lastEspNowHeartbeat = 0;
const unsigned long MEASURE_INTERVAL_MS = 200;
// Uses the shared EspNowTiming.h constant (20s) rather than a hardcoded
// value, so this device heartbeats on the same cadence as every other
// sender (train_ctrl_c3, ramzor, etc.) - was previously a mismatched
// hardcoded 30s.

void loop() {
  if (every(1000, lastHeartbeat)) {
    dbgPrintln("I am alive - " + bootStatus);
  }

  if (every(HEARTBEAT_INTERVAL_MS, lastEspNowHeartbeat)) {
    sendDistanceMessage(lastReportedMm, lastReportedValid, REASON_HEARTBEAT);
  }

  if (every(MEASURE_INTERVAL_MS, lastMeasurement)) {
    bool present = i2cPresent(TOF_ADDR);

    if (present && !tofReady) {
      // Just (re)appeared - run its proper init before trusting it.
      tofReady = tof.begin(TOF_ADDR, false, &Wire);
      dbgPrintln(tofReady ? "VL53L0X (re)connected" : "VL53L0X present but init failed");
    } else if (!present && tofReady) {
      tofReady = false;
      dbgPrintln("VL53L0X connection lost");
    }

    if (tofReady) {
      VL53L0X_RangingMeasurementData_t measure;
      tof.rangingTest(&measure, false);  // blocks briefly (~30-50ms) - fine at this rate

      char buf[16];
      if (measure.RangeStatus != 4) {  // 4 = out of range / invalid reading
        int mm = measure.RangeMilliMeter;
        // Screen shows cm.mm (2 digits cm, 1 digit mm) for readability -
        // Serial keeps raw mm as the authoritative value, e.g. for
        // future ESP-NOW use where full precision matters more than
        // a compact human-readable string.
        snprintf(buf, sizeof(buf), "%d.%dcm", mm / 10, mm % 10);
        char distMsg[32];
        snprintf(distMsg, sizeof(distMsg), "Distance: %d mm", mm);
        dbgPrintln(distMsg);

        int drift = mm - (int)lastReportedMm;
        if (drift < 0) drift = -drift;
        if (!lastReportedValid || drift >= REPORT_CHANGE_THRESHOLD_MM) {
          sendDistanceMessage((uint16_t)mm, true, REASON_STATE_CHANGE);
          lastReportedMm = (uint16_t)mm;
          lastReportedValid = true;
        }
      } else {
        snprintf(buf, sizeof(buf), "----");
        dbgPrintln("Distance: out of range");
        if (lastReportedValid) {
          sendDistanceMessage(0, false, REASON_STATE_CHANGE);
          lastReportedValid = false;
        }
      }
      showCentered(buf);
    } else {
      static int counter = 0;
      counter++;
      char buf[8];
      snprintf(buf, sizeof(buf), "%d", counter);
      showCentered(buf);  // incrementing number proves the display is
                           // still actively updating, not just frozen
      char notConnMsg[48];
      snprintf(notConnMsg, sizeof(notConnMsg), "VL53L0X: not connected (display counter: %d)", counter);
      dbgPrintln(notConnMsg);
    }
  }
}
