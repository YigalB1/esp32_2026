// train_ctrl_c3 - ESP32-C3 SuperMini firmware
//
// TEST-MODE BUILD: this revision does NOT listen for commands from the
// bridge yet (the bridge currently has no send path anyway - see DESIGN.md).
// Instead, on power-up it:
//   1. Sends a MSG_TRAIN_CONTROL / REASON_BOOT message to the bridge
//   2. Flashes all 3 LEDs for 2 seconds (visual "I'm alive" check)
//   3. Runs an endless self-test loop on Motor 1:
//        forward, ramping 0->100% over 30s (LED green)
//        -> brake, pause (LED red)
//        -> backward, ramping 0->100% over 30s (LED yellow)
//        -> brake, pause (LED red)
//        -> repeat
//
// Once real commands are wired up from the bridge, this test loop gets
// replaced by the ESP-NOW receive handler described in DESIGN.md.

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Adafruit_AW9523.h>
#include "../../shared/EspNowProtocol.h"

// ---------------- AW9523 pin map (see DESIGN.md) ----------------
#define PIN_LED_RED     0  // P0_0
#define PIN_LED_GREEN   1  // P0_1
#define PIN_LED_YELLOW  2  // P0_2
#define PIN_MOTOR_STBY  4  // P0_4 - shared standby for both motor driver ICs
#define PIN_MOTOR1_IN1  5  // P0_5
#define PIN_MOTOR1_IN2  6  // P0_6

// AW9523 I2C address: ASSUMED 0x58 (AD0=GND, AD1=GND per schematic).
// VERIFY against the actual board - if it doesn't ACK, run an I2C scanner.
#define AW9523_I2C_ADDR 0x58

// ---------------- ESP-NOW ----------------
uint8_t bridgeMac[] = {0xCC, 0xDB, 0xA7, 0x69, 0x97, 0xDC};

Adafruit_AW9523 aw;

// ---------------- Test-sequence tuning ----------------
const unsigned long RAMP_MS      = 30000; // 0 -> 100% over 30s
const unsigned long STOP_PAUSE_MS = 2000; // pause at each end before reversing
const unsigned long TICK_MS      = 100;   // control loop update rate
const uint8_t MAX_DUTY = 255;             // AW9523 8-bit PWM

enum TestState { TS_FORWARD_RAMP, TS_STOP_AFTER_FWD, TS_BACKWARD_RAMP, TS_STOP_AFTER_BCK };
TestState testState = TS_FORWARD_RAMP;
unsigned long stateStartMs = 0;

// ---------------- LED helpers ----------------
// which: 0 = red (stopped), 1 = green (forward), 2 = yellow (backward)
void setLed(int which) {
  aw.analogWrite(PIN_LED_RED,    which == 0 ? MAX_DUTY : 0);
  aw.analogWrite(PIN_LED_GREEN,  which == 1 ? MAX_DUTY : 0);
  aw.analogWrite(PIN_LED_YELLOW, which == 2 ? MAX_DUTY : 0);
}

void allLeds(bool on) {
  uint8_t v = on ? MAX_DUTY : 0;
  aw.analogWrite(PIN_LED_RED, v);
  aw.analogWrite(PIN_LED_GREEN, v);
  aw.analogWrite(PIN_LED_YELLOW, v);
}

// ---------------- Motor helpers (Motor 1 only) ----------------
void motorForward(uint8_t duty) {
  aw.digitalWrite(PIN_MOTOR_STBY, HIGH);
  aw.analogWrite(PIN_MOTOR1_IN1, duty);
  aw.analogWrite(PIN_MOTOR1_IN2, 0);
}

void motorBackward(uint8_t duty) {
  aw.digitalWrite(PIN_MOTOR_STBY, HIGH);
  aw.analogWrite(PIN_MOTOR1_IN1, 0);
  aw.analogWrite(PIN_MOTOR1_IN2, duty);
}

void motorBrake() {
  aw.digitalWrite(PIN_MOTOR_STBY, HIGH);
  aw.analogWrite(PIN_MOTOR1_IN1, MAX_DUTY);
  aw.analogWrite(PIN_MOTOR1_IN2, MAX_DUTY);
}

void motorIdle() {
  aw.digitalWrite(PIN_MOTOR_STBY, LOW);
  aw.analogWrite(PIN_MOTOR1_IN1, 0);
  aw.analogWrite(PIN_MOTOR1_IN2, 0);
}

// ---------------- ESP-NOW boot message ----------------
void sendBootMessage() {
  EspNowMessage msg = {};
  msg.type = MSG_TRAIN_CONTROL;
  msg.payload.trainControl.direction = 0;
  msg.payload.trainControl.speed = 0;
  msg.payload.trainControl.location = 0.0f;
  msg.payload.trainControl.reason = REASON_BOOT;
  esp_err_t result = esp_now_send(bridgeMac, (uint8_t *)&msg, sizeof(msg));
  Serial.printf("[boot] esp_now_send result=%d\n", result);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("[train_ctrl_c3] booting (TEST MODE)");

  // I2C on GPIO8 (SDA) / GPIO9 (SCL) per schematic
  Wire.begin(8, 9);

  if (!aw.begin(AW9523_I2C_ADDR)) {
    Serial.println("[error] AW9523 not found - check wiring/address, halting motor test");
    while (true) { delay(1000); }
  }

  aw.pinMode(PIN_LED_RED, OUTPUT);
  aw.pinMode(PIN_LED_GREEN, OUTPUT);
  aw.pinMode(PIN_LED_YELLOW, OUTPUT);
  aw.pinMode(PIN_MOTOR_STBY, OUTPUT);
  aw.pinMode(PIN_MOTOR1_IN1, OUTPUT);
  aw.pinMode(PIN_MOTOR1_IN2, OUTPUT);

  allLeds(false);
  motorIdle();

  // ---- ESP-NOW setup + boot announcement ----
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("[error] esp_now_init failed - continuing without telemetry");
  } else {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, bridgeMac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("[error] esp_now_add_peer failed");
    } else {
      sendBootMessage();
    }
  }

  // ---- Power-up LED flash: all on for 2s, then off ----
  allLeds(true);
  delay(2000);
  allLeds(false);

  Serial.println("[train_ctrl_c3] starting motor self-test loop");
  stateStartMs = millis();
  testState = TS_FORWARD_RAMP;
}

void loop() {
  unsigned long elapsed = millis() - stateStartMs;

  switch (testState) {
    case TS_FORWARD_RAMP: {
      setLed(1); // green
      float frac = min(1.0f, (float)elapsed / (float)RAMP_MS);
      motorForward((uint8_t)(frac * MAX_DUTY));
      if (elapsed >= RAMP_MS) {
        testState = TS_STOP_AFTER_FWD;
        stateStartMs = millis();
      }
      break;
    }

    case TS_STOP_AFTER_FWD: {
      setLed(0); // red
      motorBrake();
      if (elapsed >= STOP_PAUSE_MS) {
        testState = TS_BACKWARD_RAMP;
        stateStartMs = millis();
      }
      break;
    }

    case TS_BACKWARD_RAMP: {
      setLed(2); // yellow
      float frac = min(1.0f, (float)elapsed / (float)RAMP_MS);
      motorBackward((uint8_t)(frac * MAX_DUTY));
      if (elapsed >= RAMP_MS) {
        testState = TS_STOP_AFTER_BCK;
        stateStartMs = millis();
      }
      break;
    }

    case TS_STOP_AFTER_BCK: {
      setLed(0); // red
      motorBrake();
      if (elapsed >= STOP_PAUSE_MS) {
        testState = TS_FORWARD_RAMP;
        stateStartMs = millis();
      }
      break;
    }
  }

  delay(TICK_MS);
}
