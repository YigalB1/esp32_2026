/*
  Traffic Light Toy — button-driven state machine + ESP-NOW reporting

  On boot: all 3 LEDs blink together 2 times (0.5s on / 0.5s off) as a
  power-on self-test. Then settles on RED and sends a "boot" ESP-NOW message.

  After that: each button press advances to the next light:
  RED -> YELLOW -> GREEN -> YELLOW -> RED -> ...
  Every time the light changes, a "state change" ESP-NOW message is sent.
  Every 30s, an "I'm still alive" heartbeat is also sent, independent of
  button presses - reports the current light color, same as a real state
  change would, so the PC dashboard can tell this device is still running
  even during long stretches with no button activity.

  IMPORTANT - ESP8266 vs ESP32 ESP-NOW API difference: this uses the
  ESP8266's <espnow.h> API (esp_now_set_self_role, esp_now_add_peer with a
  role argument, etc.) - this is DIFFERENT from the ESP32's <esp_now.h> API
  used on the bridge. Don't copy-paste ESP-NOW code between the two chips
  without checking - the function signatures don't match.

  Confirmed pins (2026-07-17):
    Red LED    -> D6
    Yellow LED -> D7
    Green LED  -> D8
    Button     -> D5  (10k resistor D5->3.3V, button D5->GND)
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
extern "C" {
  #include <espnow.h>
}
#include "../../shared/EspNowProtocol.h"

#define PIN_LED_RED     D6
#define PIN_LED_YELLOW  D7
#define PIN_LED_GREEN   D8
#define PIN_BUTTON      D5

const unsigned long BLINK_TIME  = 500; // ms, for startup self-test
const unsigned long DEBOUNCE_MS = 40;  // ms, ignore changes faster than this
const unsigned long HEARTBEAT_INTERVAL_MS = 30000;

// ---------------- ESP-NOW target ----------------
// Bridge #2's MAC address: CC:DB:A7:69:97:DC
// (There's also a Bridge #3 planned for later - not wired up yet.)
uint8_t bridgeMac[] = {0xCC, 0xDB, 0xA7, 0x69, 0x97, 0xDC};

enum LightState { RED, YELLOW_TO_GREEN, GREEN, YELLOW_TO_RED };
LightState state = RED;

int lastRawReading = HIGH;
int stableState    = HIGH;
unsigned long lastChangeTime = 0;
unsigned long lastHeartbeatMs = 0;

void setAllLeds(bool on) {
  digitalWrite(PIN_LED_RED, on ? HIGH : LOW);
  digitalWrite(PIN_LED_YELLOW, on ? HIGH : LOW);
  digitalWrite(PIN_LED_GREEN, on ? HIGH : LOW);
}

void showState() {
  bool yellowOn = (state == YELLOW_TO_GREEN || state == YELLOW_TO_RED);
  digitalWrite(PIN_LED_RED,    state == RED);
  digitalWrite(PIN_LED_YELLOW, yellowOn);
  digitalWrite(PIN_LED_GREEN,  state == GREEN);
}

// Maps the state machine's 4 internal states down to the 3 colors a
// receiver actually cares about - both yellow transitions report as 'Y'.
char stateToChar(LightState s) {
  if (s == RED) return 'R';
  if (s == GREEN) return 'G';
  return 'Y';
}

// ---------------- ESP-NOW send delivery confirmation ----------------
// esp_now_send()'s return value only means "queued", not "delivered". On a
// cold boot the very first ESP-NOW packet can be silently dropped because
// the WiFi radio isn't fully settled yet, even though the call reports
// success - same issue seen and fixed on train_ctrl_c3. Use the actual
// delivery-confirmation callback and retry the boot message a few times.
// NOTE: ESP8266's callback signature differs from ESP32's - takes a
// uint8_t status (0 = success), not an esp_now_send_status_t enum.
volatile bool sendComplete = false;
volatile bool lastSendSuccess = false;

void onDataSent(uint8_t *mac_addr, uint8_t status) {
  lastSendSuccess = (status == 0);
  sendComplete = true;
}

// ---------------- ESP-NOW sending ----------------
void sendTrafficLightMsgOnce(EspNowReason reason) {
  EspNowMessage msg;
  memset(&msg, 0, sizeof(msg));
  msg.type = MSG_TRAFFIC_LIGHT;
  msg.payload.trafficLight.state = stateToChar(state);
  msg.payload.trafficLight.reason = reason;

  esp_now_send(bridgeMac, (uint8_t*)&msg, sizeof(msg));
}

// Boot message specifically gets delivery-confirmed retries, since it's the
// one most likely to hit the cold-start radio-not-ready issue.
void sendBootMessageWithRetry() {
  const int maxAttempts = 4;
  for (int attempt = 1; attempt <= maxAttempts; attempt++) {
    sendComplete = false;
    lastSendSuccess = false;

    EspNowMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_TRAFFIC_LIGHT;
    msg.payload.trafficLight.state = stateToChar(state);
    msg.payload.trafficLight.reason = REASON_BOOT;
    int result = esp_now_send(bridgeMac, (uint8_t*)&msg, sizeof(msg));
    Serial.printf("[boot] esp_now_send attempt %d, queue result=%d\n", attempt, result);

    unsigned long waitStart = millis();
    while (!sendComplete && millis() - waitStart < 200) {
      delay(5);
    }

    if (lastSendSuccess) {
      Serial.printf("[boot] delivery confirmed on attempt %d\n", attempt);
      return;
    }
    delay(150); // brief pause before retrying - lets the radio settle further
  }
  Serial.println("[boot] warning: delivery not confirmed after all retries");
}

void setupEspNow() {
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  // ESP8266 API: must declare our own role, then register the peer with its
  // role, before esp_now_send() will work.
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_add_peer(bridgeMac, ESP_NOW_ROLE_SLAVE, 1, NULL, 0); // channel 1 - must
                                                                  // match the
                                                                  // bridge's WiFi
                                                                  // channel
  esp_now_register_send_cb(onDataSent);
}

void nextState() {
  switch (state) {
    case RED:             state = YELLOW_TO_GREEN; break;
    case YELLOW_TO_GREEN: state = GREEN;            break;
    case GREEN:           state = YELLOW_TO_RED;    break;
    case YELLOW_TO_RED:   state = RED;              break;
  }
  showState();
  sendTrafficLightMsgOnce(REASON_STATE_CHANGE);
  lastHeartbeatMs = millis(); // a real state change also resets the heartbeat clock
}

// Returns true exactly once, on the debounced press edge (HIGH -> LOW)
bool checkButtonPressed() {
  int reading = digitalRead(PIN_BUTTON);
  bool pressedEdge = false;

  if (reading != lastRawReading) {
    lastChangeTime = millis();
  }

  if ((millis() - lastChangeTime) > DEBOUNCE_MS) {
    if (reading != stableState) {
      stableState = reading;
      if (stableState == LOW) {
        pressedEdge = true; // clean press detected
      }
    }
  }

  lastRawReading = reading;
  return pressedEdge;
}

void setup() {
  Serial.begin(115200); // matches the bridge and train_ctrl_c3's baud rate

  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_BUTTON, INPUT); // external 10k pull-up to 3.3V; button pulls to GND

  // Startup self-test: blink all LEDs together, 2 times
  for (int i = 0; i < 2; i++) {
    setAllLeds(true);
    delay(BLINK_TIME);
    setAllLeds(false);
    delay(BLINK_TIME);
  }

  // Start on RED
  state = RED;
  showState();

  setupEspNow();

  // Print our own MAC, same style/purpose as the bridge's boot print - lets
  // you confirm the MAC to add to the bridge's knownDevices[] table.
  Serial.print("{\"event\":\"boot\",\"mac\":\"");
  Serial.print(WiFi.macAddress());
  Serial.println("\"}");

  sendBootMessageWithRetry();
  lastHeartbeatMs = millis();

  Serial.println("Ready. Press the button to advance the light.");
}

void loop() {
  if (checkButtonPressed()) {
    nextState();
    Serial.println("Button pressed -> light advanced");
  }

  if (millis() - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = millis();
    sendTrafficLightMsgOnce(REASON_HEARTBEAT);
  }
}
