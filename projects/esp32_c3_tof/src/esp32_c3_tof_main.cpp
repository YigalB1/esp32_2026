// esp32_c3_tof_main.cpp
//
// Stage 0: OLED display bring-up only. No sensor, no ESP-NOW yet -
// just proving the display itself works before anything else is
// built on top of it.
//
// Cycles through three placeholder states, 3 seconds each, forever:
//   1. a placeholder distance number
//   2. "V" - stands in for "detected"
//   3. "X" - stands in for "not detected"
//
// I2C pins CONFIRMED by the boot scan: GPIO5 (SDA) / GPIO6 (SCL),
// device found at address 0x3C. The GPIO8/9 guess (suggested by this
// board's silkscreen calling out IO8 separately) was wrong.
#define OLED_SDA 5
#define OLED_SCL 6

// Pins are confirmed now - scan disabled. Flip back to 1 if this
// ever needs re-diagnosing (e.g. on a differently-wired board).
#define RUN_I2C_SCAN_AT_BOOT 0

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);

// This panel is physically 72x40 pixels, sitting inside a controller
// that thinks it's addressing 128x64 - without this offset, anything
// drawn near (0,0) renders off the visible area entirely.
const int PANEL_W = 72;
const int PANEL_H = 40;
const int X_OFFSET = 30;  // (132 - PANEL_W) / 2
const int Y_OFFSET = 12;  // (64 - PANEL_H) / 2

#if RUN_I2C_SCAN_AT_BOOT
struct PinPair {
  int sda;
  int scl;
  const char *label;
};

PinPair scanCandidates[] = {
  {8, 9, "GPIO8 (SDA) / GPIO9 (SCL)"},
  {5, 6, "GPIO5 (SDA) / GPIO6 (SCL)"},
};

String scanResults;  // captured once, re-printed repeatedly so it's never missed

static void scanPins(int sda, int scl, const char *label) {
  Wire.end();
  Wire.begin(sda, scl);
  delay(50);

  scanResults += String("Scanning with ") + label + " ...\n";
  bool found = false;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      char line[32];
      snprintf(line, sizeof(line), "  Found device at 0x%02X\n", addr);
      scanResults += line;
      found = true;
    }
  }
  if (!found) {
    scanResults += "  Nothing responded on this pin pair\n";
  }
}

// Runs the scan exactly ONCE - repeated Wire.end()/begin() cycling
// (e.g. calling this from loop()) is unreliable on this core and
// throws "bus not initialized" / NULL buffer errors after a pass or
// two. Results are captured to a string and re-printed from loop()
// instead, so missing the message once (e.g. due to a reset) isn't
// a problem without needing to re-touch the I2C bus at all.
static void runBootI2cScan() {
  scanResults = "I2C scan starting...\n";
  for (auto &c : scanCandidates) {
    scanPins(c.sda, c.scl, c.label);
  }
  scanResults += "I2C scan done.\n";
  Wire.end();  // leave a clean bus state before u8g2 initializes it
}
#endif

static void showCentered(const char *text) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso22_tr);
  int w = u8g2.getStrWidth(text);
  int x = X_OFFSET + (PANEL_W - w) / 2;
  int y = Y_OFFSET + PANEL_H / 2 + 10;  // rough vertical centering for this font's baseline
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

void setup() {
  Serial.begin(115200);
  delay(3000);  // give native-USB CDC a moment to actually connect

  Serial.println("=== BOOT ===");
#if RUN_I2C_SCAN_AT_BOOT
  Serial.println("RUN_I2C_SCAN_AT_BOOT = 1 (scan enabled)");
#else
  Serial.println("RUN_I2C_SCAN_AT_BOOT = 0 (scan disabled)");
#endif

#if RUN_I2C_SCAN_AT_BOOT
  runBootI2cScan();
#endif

  u8g2.begin();
  u8g2.setContrast(255);
  u8g2.setBusClock(400000);

  Serial.println("=== setup() complete, entering loop() ===");
}

// Non-blocking display cycle: instead of delay()-ing through each
// state in sequence (which would block Serial handling, sensor
// polling, or ESP-NOW sends once those exist), track elapsed time
// and advance state without ever halting loop() itself.
enum DisplayState { SHOW_DISTANCE, SHOW_DETECTED, SHOW_NOT_DETECTED };
DisplayState displayState = SHOW_DISTANCE;
unsigned long lastStateChange = 0;
const unsigned long STATE_INTERVAL_MS = 3000;

#if RUN_I2C_SCAN_AT_BOOT
unsigned long lastScanPrint = 0;
const unsigned long SCAN_PRINT_INTERVAL_MS = 3000;
#endif

unsigned long lastHeartbeat = 0;

void loop() {
  unsigned long now = millis();

  if (every(1000, lastHeartbeat)) {
    Serial.println("I am alive");
  }

#if RUN_I2C_SCAN_AT_BOOT
  if (now - lastScanPrint >= SCAN_PRINT_INTERVAL_MS) {
    lastScanPrint = now;
    Serial.println("---- I2C scan results (captured once at boot) ----");
    Serial.print(scanResults);
  }
#endif

  if (now - lastStateChange >= STATE_INTERVAL_MS) {
    lastStateChange = now;

    switch (displayState) {
      case SHOW_DISTANCE:
        showCentered("123mm");  // placeholder distance - real sensor comes later
        displayState = SHOW_DETECTED;
        break;
      case SHOW_DETECTED:
        showCentered("V");  // placeholder for "detected"
        displayState = SHOW_NOT_DETECTED;
        break;
      case SHOW_NOT_DETECTED:
        showCentered("X");  // placeholder for "not detected"
        displayState = SHOW_DISTANCE;
        break;
    }
  }
}
