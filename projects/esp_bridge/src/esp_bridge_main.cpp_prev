// esp_bridge - ESP32 firmware
//
// THIS DEVICE: Bridge #2, MAC CC:DB:A7:69:97:DC. A Bridge #3 is planned for
// later (not wired up yet) - if/when it's added, remember each sender device
// needs its own bridgeMac target configured to whichever bridge it should
// report to.
//
// General-purpose bridge - not specific to any one device type (traffic
// light, train control, etc.). Sits on a bare ESP32, connected via USB to a Windows laptop. Listens for
// ESP-NOW messages from other devices (traffic lights, train control, etc.),
// looks up the sender's MAC in a known-devices table to get a friendly name,
// and forwards each message to the laptop as one JSON line over Serial.
//
// NOTE: this firmware currently only RECEIVES over ESP-NOW - there is no
// send path back out. If/when commands need to be relayed to devices like
// train_ctrl_c3, that capability doesn't exist here yet.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>
#include "../../shared/EspNowProtocol.h"

// ---------------- Known devices (MAC -> friendly name) ----------------
struct KnownDevice {
  uint8_t mac[6];
  const char* name;
};

KnownDevice knownDevices[] = {
  {{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01}, "traffic_lights1"}, // PLACEHOLDER - replace with real MAC
  {{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x02}, "train_gate3"},      // PLACEHOLDER - replace with real MAC
  // TODO: add train_ctrl_c3's MAC here once known (read it via
  // WiFi.macAddress() in train_ctrl_c3's own setup(), same way this bridge
  // prints its own MAC below). Until then it will show up as "unknown_<mac>".
  // {{0x??, 0x??, 0x??, 0x??, 0x??, 0x??}, "train_ctrl_c3"},
};
const int numKnownDevices = sizeof(knownDevices) / sizeof(knownDevices[0]);

String macToHex(const uint8_t *mac) {
  char buf[13];
  snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

String lookupDeviceName(const uint8_t *mac) {
  for (int i = 0; i < numKnownDevices; i++) {
    if (memcmp(mac, knownDevices[i].mac, 6) == 0) {
      return String(knownDevices[i].name);
    }
  }
  return "unknown_" + macToHex(mac);
}

const char* reasonToStr(uint8_t reason) {
  switch (reason) {
    case REASON_BOOT: return "boot";
    case REASON_STATE_CHANGE: return "state_change";
    default: return "unknown_reason";
  }
}

unsigned long lastActivity = 0; // reset whenever a real ESP-NOW message arrives

void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len) {
  lastActivity = millis(); // real activity - the idle dot timer restarts from here

  String deviceName = lookupDeviceName(mac_addr);

  if (len < 1 || (size_t)len > sizeof(EspNowMessage)) {
    Serial.println(); // break out of any accumulated dot-line first
    Serial.print("{\"device\":\"");
    Serial.print(deviceName);
    Serial.println("\",\"type\":\"malformed\"}");
    return;
  }

  EspNowMessage msg;
  memset(&msg, 0, sizeof(msg));
  memcpy(&msg, data, len);

  JsonDocument doc;
  doc["device"] = deviceName;             // from whom
  doc["time_s"] = millis() / 1000.0;       // time - seconds since the BRIDGE booted,
                                            // NOT real wall-clock time (the ESP32 has
                                            // no RTC here) - fine for now since this
                                            // is just for relative ordering/debugging

  switch (msg.type) {
    case MSG_TRAFFIC_LIGHT:
      doc["type"] = "traffic_light";
      doc["state"] = String(msg.payload.trafficLight.state);
      doc["reason"] = reasonToStr(msg.payload.trafficLight.reason);
      break;

    case MSG_TRAIN_CONTROL:
      doc["type"] = "train_control";
      doc["direction"] = msg.payload.trainControl.direction;
      doc["speed"] = msg.payload.trainControl.speed;
      doc["location"] = msg.payload.trainControl.location;
      doc["reason"] = reasonToStr(msg.payload.trainControl.reason);
      break;

    default:
      doc["type"] = "unknown";
      doc["raw_type_byte"] = msg.type;
      break;
  }

  Serial.println(); // break out of any accumulated dot-line first
  serializeJson(doc, Serial);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.mode(WIFI_STA);
  Serial.print("{\"event\":\"boot\",\"bridge_mac\":\"");
  Serial.print(WiFi.macAddress());
  Serial.println("\"}");

  if (esp_now_init() != ESP_OK) {
    Serial.println("{\"event\":\"error\",\"message\":\"ESP-NOW init failed\"}");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);
  Serial.println("{\"event\":\"ready\"}");
  lastActivity = millis();
}

void loop() {
  // Idle indicator: print a single "." (no newline) every 10 seconds of
  // silence. Resets on any real ESP-NOW message, so dots only accumulate
  // during genuine idle time, not mixed in with message output.
  static unsigned long lastDot = 0;
  if (millis() - lastActivity >= 10000 && millis() - lastDot >= 10000) {
    lastDot = millis();
    Serial.print(".");
  }
}
