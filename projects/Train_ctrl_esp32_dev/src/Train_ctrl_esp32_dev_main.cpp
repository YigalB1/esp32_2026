// Train_ctrl_esp32_dev_main.cpp
//
// Single-motor train controller on an ESP32-DOIT-DEVKIT-V1, driving a
// motor via an Adafruit DRV8871 H-bridge. Communicates with the ESP-NOW
// bridge (esp_bridge) using the shared EspNowProtocol.h and
// EspNowTiming.h.
//
// Board: Train_CTRL_2024-06-11_07-31-04 rev 1.0 (partially assembled -
// only U1/ESP32, U3/DRV8871, U6/power jack, and LED3/4/5 are populated).
//
// Pin map (see DESIGN.md):
//   M1_IN1 -> GPIO19   (DRV8871 IN1, PWM)
//   M1_IN2 -> GPIO23   (DRV8871 IN2, PWM)
//   LED3   -> GPIO13   (stopped / red)   - active-LOW
//   LED4   -> GPIO25   (forward / green) - active-LOW
//   LED5   -> GPIO26   (backward / yellow) - active-LOW
//
// Direction convention (payload.trainControl.direction / trainCommand.direction):
//   -1 = backward, 0 = stop, 1 = forward
//
// Bridge peer MAC: CC:DB:A7:69:97:DC
//
// Boot sequence:
//   1. Print this board's own MAC address to Serial (needed to register
//      it as an ESP-NOW peer on the bridge side).
//   2. Send one ESP-NOW announcement (MSG_TRAIN_CONTROL, REASON_BOOT).
//   3. Flash all LEDs for 2 seconds.
//   4. Enter MODE_AUTO: run an endless Motor 1 self-test loop (forward
//      ramp 0->100% over 30s, brake/pause 2s, backward ramp 0->100% over
//      30s, brake/pause 2s, repeat) until commanded otherwise.
//
// Control modes (see MSG_TRAIN_COMMAND in EspNowProtocol.h):
//   MODE_AUTO   - run the self-test loop above (default at boot).
//   MODE_MANUAL - drive exactly what the dashboard last commanded
//                 (running/direction/speed).
//
// Watchdog: the dashboard is the "brain" and owns the policy of how long
// is too long without contact - it sends the threshold (watchdogSeconds)
// with every command. This device is the only thing still running once
// the link is actually down, though, so it's the one that has to enforce
// the cutoff: if no MSG_TRAIN_COMMAND arrives for watchdogMs, in EITHER
// mode, the motor is force-stopped and the self-test (if in MODE_AUTO) is
// paused until a fresh command arrives. Defaults to 20 minutes at boot,
// before the dashboard has ever sent a value.
//
// Liveness: sends a REASON_HEARTBEAT message every HEARTBEAT_INTERVAL_MS
// (see EspNowTiming.h) reporting current direction/speed, independent of
// any real state change. This is why the main loop is written as a
// non-blocking millis()-based state machine rather than using delay() -
// a blocking delay (e.g. during the 2s brake pause) would be able to
// starve the heartbeat schedule.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "../../shared/EspNowProtocol.h"
#include "../../shared/EspNowTiming.h"

// ---------------------------------------------------------------------
// Pin map
// ---------------------------------------------------------------------
static const uint8_t PIN_M1_IN1 = 19;
static const uint8_t PIN_M1_IN2 = 23;
static const uint8_t PIN_LED_STOPPED  = 13; // LED3, red
static const uint8_t PIN_LED_FORWARD  = 25; // LED4, green
static const uint8_t PIN_LED_BACKWARD = 26; // LED5, yellow
static const uint8_t PIN_TEMP_SENSOR   = 32; // D32, DS18B20 data (4.7k pull-up to 3.3V)

// PWM (LEDC) channels/config for the motor driver inputs
static const int PWM_FREQ_HZ   = 20000; // 20kHz, above audible range
static const int PWM_RES_BITS  = 8;     // 0-255 duty, matches protocol's speed range
static const int PWM_CH_IN1    = 0;
static const int PWM_CH_IN2    = 1;

// ---------------------------------------------------------------------
// ESP-NOW peer (bridge)
// ---------------------------------------------------------------------
static uint8_t bridgeAddress[6] = {0xCC, 0xDB, 0xA7, 0x69, 0x97, 0xDC}; // bridge MAC (confirmed via bridge boot log)

// ---------------------------------------------------------------------
// Self-test timing (matches train_ctrl_c3's Motor 1 self-test shape)
// ---------------------------------------------------------------------
static const uint32_t RAMP_DURATION_MS = 30000; // 0 -> 100% over 30s
static const uint32_t BRAKE_PAUSE_MS   = 2000;  // pause between ramps

// ---------------------------------------------------------------------
// Watchdog: threshold is owned by the dashboard (sent with every
// command via watchdogSeconds); this is just the boot-time default,
// used until the dashboard has sent a value at all.
// ---------------------------------------------------------------------
static uint32_t watchdogMs = 20UL * 60UL * 1000UL; // 20 minutes

// ---------------------------------------------------------------------
// Current reported state (used both for driving hardware and for
// whatever the next heartbeat/state-change message reports)
// ---------------------------------------------------------------------
static int8_t  currentDirection = 0; // -1/0/1
static uint8_t currentSpeed = 0;     // 0-255

// ---------------------------------------------------------------------
// Control mode + manual command state
// ---------------------------------------------------------------------
static uint8_t controlMode = MODE_AUTO; // one of ControlMode
static bool    manualRunning = false;
static int8_t  manualDirection = 0;
static uint8_t manualSpeed = 0;
static uint32_t lastCommandMs = 0;   // last time ANY MSG_TRAIN_COMMAND arrived, either mode
static bool     watchdogTripped = false;

// ---------------------------------------------------------------------
// Self-test state machine
// ---------------------------------------------------------------------
enum SelfTestState {
  ST_FORWARD_RAMP,
  ST_FORWARD_BRAKE,
  ST_BACKWARD_RAMP,
  ST_BACKWARD_BRAKE,
};
static SelfTestState selfTestState = ST_FORWARD_RAMP;
static uint32_t selfTestStateStartMs = 0;

// ---------------------------------------------------------------------
// Heartbeat scheduling
// ---------------------------------------------------------------------
static uint32_t lastHeartbeatMs = 0;

// ---------------------------------------------------------------------
// DS18B20 temperature sensing. Read on a separate low-priority FreeRTOS
// task so the blocking ~750ms conversion wait never stalls loop()'s
// watchdog/heartbeat/motor timing. The task only ever touches the
// sensor and the queue below - it must NOT call esp_now_send or Serial;
// loop() owns all sending and all Serial output for this reading.
// ---------------------------------------------------------------------
static OneWire oneWire(PIN_TEMP_SENSOR);
static DallasTemperature tempSensors(&oneWire);
static QueueHandle_t tempQueue = nullptr;

struct TempReading {
  float   tempC;
  uint8_t valid; // 0 = sensor missing or read failed (-127 or the 85.0 power-on default)
};

static const uint32_t TEMP_SAMPLE_INTERVAL_MS = 5000;
static const uint32_t TEMP_BOOT_TIMEOUT_MS    = 5000; // how long to wait for a first reading before reporting valid=0 at boot

void tempSensorTask(void *pvParameters) {
  tempSensors.begin();
  tempSensors.setWaitForConversion(false); // we drive the wait ourselves with vTaskDelay below
  for (;;) {
    tempSensors.requestTemperatures();
    vTaskDelay(pdMS_TO_TICKS(800)); // 12-bit conversion takes up to ~750ms
    float c = tempSensors.getTempCByIndex(0);

    TempReading reading;
    if (c == DEVICE_DISCONNECTED_C || c == 85.0f) {
      reading.tempC = 0.0f;
      reading.valid = 0;
    } else {
      reading.tempC = c;
      reading.valid = 1;
    }
    xQueueOverwrite(tempQueue, &reading);

    vTaskDelay(pdMS_TO_TICKS(TEMP_SAMPLE_INTERVAL_MS));
  }
}

// Latest sample drained from the queue, regardless of whether it was ever sent.
static TempReading tempLatest = {0.0f, 0};

// Last reading this firmware has actually SENT over ESP-NOW (not just the
// latest sample) - REASON_STATE_CHANGE compares against this, not against
// whatever the sensor task last produced.
static bool    tempBootSent      = false;
static float   tempLastSentC     = 0.0f;
static uint8_t tempLastSentValid = 0;
static uint32_t tempBootDeadlineMs = 0;

// ---------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------

void printOwnMac() {
  Serial.println();
  Serial.println("=====================================");
  Serial.print("This board's MAC address: ");
  Serial.println(WiFi.macAddress());
  Serial.println("(register this as an ESP-NOW peer on the bridge)");
  Serial.println("=====================================");
}

void setAllLeds(bool stopped, bool forward, bool backward) {
  // LED3/4/5 are active-low on this board: LOW turns the LED on.
  digitalWrite(PIN_LED_STOPPED, stopped ? LOW : HIGH);
  digitalWrite(PIN_LED_FORWARD, forward ? LOW : HIGH);
  digitalWrite(PIN_LED_BACKWARD, backward ? LOW : HIGH);
}

// direction: -1 = backward, 0 = stop, 1 = forward
// speed: 0-255 PWM duty
void driveMotor(int8_t direction, uint8_t speed) {
  currentDirection = direction;
  currentSpeed = speed;

  if (direction > 0) {
    ledcWrite(PWM_CH_IN1, speed);
    ledcWrite(PWM_CH_IN2, 0);
    setAllLeds(false, true, false);
  } else if (direction < 0) {
    ledcWrite(PWM_CH_IN1, 0);
    ledcWrite(PWM_CH_IN2, speed);
    setAllLeds(false, false, true);
  } else {
    // stop / brake: both inputs high (fast-decay brake per DRV8871 datasheet)
    ledcWrite(PWM_CH_IN1, 255);
    ledcWrite(PWM_CH_IN2, 255);
    setAllLeds(true, false, false);
  }
}

void sendEspNowMessage(int8_t direction, uint8_t speed, EspNowReason reason) {
  EspNowMessage msg;
  memset(&msg, 0, sizeof(msg));
  msg.type = MSG_TRAIN_CONTROL;
  msg.payload.trainControl.direction = direction;
  msg.payload.trainControl.speed = speed;
  msg.payload.trainControl.location = 0.0f; // no position sensing on this board
  msg.payload.trainControl.reason = reason;

  esp_err_t result = esp_now_send(bridgeAddress, (uint8_t *)&msg, sizeof(msg));
  if (result != ESP_OK) {
    Serial.print("ESP-NOW send failed, err=");
    Serial.println(result);
  }
}

void onEspNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Lightweight debug hook - not required for operation.
  if (status != ESP_NOW_SEND_SUCCESS) {
    Serial.println("ESP-NOW: last send failed");
  }
}

void sendTemperatureMessage(float tempC, uint8_t valid, EspNowReason reason) {
  EspNowMessage msg;
  memset(&msg, 0, sizeof(msg));
  msg.type = MSG_TEMPERATURE;
  msg.payload.temperature.tempCentiC = (int16_t)lroundf(tempC * 100.0f);
  msg.payload.temperature.valid = valid;
  msg.payload.temperature.reason = reason;

  esp_err_t result = esp_now_send(bridgeAddress, (uint8_t *)&msg, sizeof(msg));
  if (result != ESP_OK) {
    Serial.print("ESP-NOW send failed (temperature), err=");
    Serial.println(result);
  }
}

// Handles an incoming MSG_TRAIN_COMMAND from the bridge (originally from
// the dashboard). Switches mode and/or updates the manual drive targets.
void onEspNowDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len) {
  if (len < 1 || (size_t)len > sizeof(EspNowMessage)) {
    return;
  }
  EspNowMessage msg;
  memset(&msg, 0, sizeof(msg));
  memcpy(&msg, data, len);

  if (msg.type != MSG_TRAIN_COMMAND) {
    return; // not for us / not a command this firmware understands
  }

  uint8_t newMode = msg.payload.trainCommand.mode;
  bool wasTripped = watchdogTripped;

  lastCommandMs = millis();
  watchdogTripped = false; // any command, either mode, counts as contact

  if (msg.payload.trainCommand.watchdogSeconds > 0) {
    watchdogMs = (uint32_t)msg.payload.trainCommand.watchdogSeconds * 1000UL;
  }

  if (newMode == MODE_AUTO) {
    if (controlMode != MODE_AUTO || wasTripped) {
      // Just switched into auto (or resuming after a watchdog trip) -
      // restart the self-test cleanly rather than resuming from a stale
      // elapsed-time calculation.
      selfTestState = ST_FORWARD_RAMP;
      selfTestStateStartMs = millis();
    }
    controlMode = MODE_AUTO;
    manualRunning = false;
  } else {
    controlMode = MODE_MANUAL;
    manualRunning = (msg.payload.trainCommand.running != 0);
    manualDirection = msg.payload.trainCommand.direction;
    manualSpeed = msg.payload.trainCommand.speed;
    if (!manualRunning) {
      driveMotor(0, 0);
    } else {
      driveMotor(manualDirection, manualSpeed);
    }
  }

  // Confirm the new state back to the dashboard immediately, rather than
  // waiting for the next heartbeat.
  sendEspNowMessage(currentDirection, currentSpeed, REASON_STATE_CHANGE);
}

// Called every loop() iteration - fires a heartbeat if due, independent
// of whatever mode/state machine is currently driving the motor.
void checkHeartbeat() {
  uint32_t now = millis();
  if (now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = now;
    sendEspNowMessage(currentDirection, currentSpeed, REASON_HEARTBEAT);
    if (tempBootSent) {
      sendTemperatureMessage(tempLatest.tempC, tempLatest.valid, REASON_HEARTBEAT);
      tempLastSentC = tempLatest.tempC;
      tempLastSentValid = tempLatest.valid;
    }
  }
}

// Called every loop() iteration. Drains the latest sample (if any) from
// the sensor task's length-1 queue, prints it to Serial, and decides
// whether it's worth sending: REASON_BOOT for the first reading (or
// valid=0 if the sensor hasn't answered within TEMP_BOOT_TIMEOUT_MS of
// boot), then REASON_STATE_CHANGE whenever the value moves 0.5C or more
// from the last SENT value or validity flips. All ESP-NOW sending and
// Serial printing for temperature happens here, never in the task.
void checkTemperature() {
  TempReading reading;
  if (xQueueReceive(tempQueue, &reading, 0) == pdTRUE) {
    tempLatest = reading;
    Serial.print("DS18B20: ");
    if (reading.valid) {
      Serial.print(reading.tempC, 2);
      Serial.println(" C");
    } else {
      Serial.println("invalid/no sensor");
    }

    if (!tempBootSent) {
      sendTemperatureMessage(reading.tempC, reading.valid, REASON_BOOT);
      tempLastSentC = reading.tempC;
      tempLastSentValid = reading.valid;
      tempBootSent = true;
    } else {
      bool validityChanged = (reading.valid != tempLastSentValid);
      bool valueChanged = reading.valid && fabsf(reading.tempC - tempLastSentC) >= 0.5f;
      if (validityChanged || valueChanged) {
        sendTemperatureMessage(reading.tempC, reading.valid, REASON_STATE_CHANGE);
        tempLastSentC = reading.tempC;
        tempLastSentValid = reading.valid;
      }
    }
  } else if (!tempBootSent && millis() >= tempBootDeadlineMs) {
    // No reading within a few seconds of boot - sensor missing or not wired yet.
    sendTemperatureMessage(0.0f, 0, REASON_BOOT);
    tempLastSentC = 0.0f;
    tempLastSentValid = 0;
    tempBootSent = true;
  }
}

// Fail-safe, applies in BOTH modes: if no MSG_TRAIN_COMMAND has arrived
// for watchdogMs (dashboard-set policy - see EspNowProtocol.h), force-stop
// the motor and pause the self-test if one was running. Stays tripped
// (and stays in whatever controlMode it was in) until a fresh command
// arrives - onEspNowDataRecv() clears watchdogTripped and, if resuming
// MODE_AUTO, restarts the self-test cleanly.
void checkWatchdog() {
  if (watchdogTripped) {
    return; // already tripped - nothing to do until a new command clears it
  }
  if (millis() - lastCommandMs >= watchdogMs) {
    watchdogTripped = true;
    manualRunning = false;
    driveMotor(0, 0);
    sendEspNowMessage(currentDirection, currentSpeed, REASON_STATE_CHANGE);
  }
}

void flashAllLeds(uint32_t durationMs) {
  // Active-low: LOW = on.
  digitalWrite(PIN_LED_STOPPED, LOW);
  digitalWrite(PIN_LED_FORWARD, LOW);
  digitalWrite(PIN_LED_BACKWARD, LOW);
  delay(durationMs); // one-time boot flash, fine to block briefly before the main loop starts
  digitalWrite(PIN_LED_STOPPED, HIGH);
  digitalWrite(PIN_LED_FORWARD, HIGH);
  digitalWrite(PIN_LED_BACKWARD, HIGH);
}

// Advances the self-test state machine by one loop() tick. Non-blocking:
// computes ramp speed from elapsed time rather than sleeping, so
// checkHeartbeat() (called separately every loop() iteration) never gets
// starved by a long delay(). Only called while controlMode == MODE_AUTO.
void updateSelfTest() {
  uint32_t elapsed = millis() - selfTestStateStartMs;

  switch (selfTestState) {
    case ST_FORWARD_RAMP: {
      if (elapsed >= RAMP_DURATION_MS) {
        driveMotor(1, 255);
        selfTestState = ST_FORWARD_BRAKE;
        selfTestStateStartMs = millis();
      } else {
        uint8_t speed = (uint8_t)((elapsed * 255UL) / RAMP_DURATION_MS);
        driveMotor(1, speed);
      }
      break;
    }
    case ST_FORWARD_BRAKE: {
      driveMotor(0, 0);
      if (elapsed >= BRAKE_PAUSE_MS) {
        selfTestState = ST_BACKWARD_RAMP;
        selfTestStateStartMs = millis();
      }
      break;
    }
    case ST_BACKWARD_RAMP: {
      if (elapsed >= RAMP_DURATION_MS) {
        driveMotor(-1, 255);
        selfTestState = ST_BACKWARD_BRAKE;
        selfTestStateStartMs = millis();
      } else {
        uint8_t speed = (uint8_t)((elapsed * 255UL) / RAMP_DURATION_MS);
        driveMotor(-1, speed);
      }
      break;
    }
    case ST_BACKWARD_BRAKE: {
      driveMotor(0, 0);
      if (elapsed >= BRAKE_PAUSE_MS) {
        selfTestState = ST_FORWARD_RAMP;
        selfTestStateStartMs = millis();
      }
      break;
    }
  }
}

// ---------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_LED_STOPPED, OUTPUT);
  pinMode(PIN_LED_FORWARD, OUTPUT);
  pinMode(PIN_LED_BACKWARD, OUTPUT);
  setAllLeds(false, false, false);

  ledcSetup(PWM_CH_IN1, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcSetup(PWM_CH_IN2, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcAttachPin(PIN_M1_IN1, PWM_CH_IN1);
  ledcAttachPin(PIN_M1_IN2, PWM_CH_IN2);
  driveMotor(0, 0); // start stopped

  WiFi.mode(WIFI_STA);
  printOwnMac();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed - halting");
    while (true) { delay(1000); }
  }
  esp_now_register_send_cb(onEspNowDataSent);
  esp_now_register_recv_cb(onEspNowDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, bridgeAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add ESP-NOW peer (bridge)");
  }

  // Boot announce
  sendEspNowMessage(0, 0, REASON_BOOT);
  lastHeartbeatMs = millis(); // don't fire a heartbeat immediately after boot announce
  lastCommandMs = millis();   // don't let the watchdog trip before the dashboard has ever connected

  // DS18B20: low-priority background task, no core pinning. loop() owns
  // all ESP-NOW sending/Serial output for readings - see checkTemperature().
  tempQueue = xQueueCreate(1, sizeof(TempReading));
  tempBootDeadlineMs = millis() + TEMP_BOOT_TIMEOUT_MS;
  xTaskCreate(tempSensorTask, "TempSensor", 4096, nullptr, 1, nullptr);

  // Flash all LEDs for 2s (one-time, before the non-blocking loop starts)
  flashAllLeds(2000);

  selfTestState = ST_FORWARD_RAMP;
  selfTestStateStartMs = millis();
}

void loop() {
  checkWatchdog();
  if (!watchdogTripped && controlMode == MODE_AUTO) {
    updateSelfTest();
  }
  // Manual driving happens directly in onEspNowDataRecv() when a command
  // arrives - nothing to do here between commands, in either mode.
  checkHeartbeat();
  checkTemperature();
}
