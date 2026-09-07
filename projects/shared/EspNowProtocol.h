// EspNowProtocol.h
//
// SHARED FILE - copy this exact file into every project that sends or
// receives these messages (traffic light, train control, bridge). All
// copies must stay identical, or the struct layouts won't match.
//
// Design: every message starts with a 1-byte type tag, so the receiver
// knows how to interpret the rest without guessing from packet size alone.
// Add a new MsgType + struct branch for each new device type in the future.
//
// CHANGELOG:
//  - Added EspNowReason (generalized from TrafficLightReason) and a new
//    `reason` field on trainControl, so train_control devices can send a
//    boot announcement the same way traffic lights do. This DOES grow the
//    trainControl struct by one byte (harmless - the bridge's `len` check
//    against sizeof(EspNowMessage) still works fine). trafficLight.reason
//    just switches to reuse EspNowReason - no layout change there, values
//    are identical to the old TrafficLightReason.
//    ACTION REQUIRED: copy this file into the traffic-light and bridge
//    projects as well.
//  - Added REASON_HEARTBEAT - a periodic "I'm still alive" message, sent on
//    a timer (e.g. every 30s) independent of any real state change. No
//    struct layout change, just a new enum value. Consumers (the PC
//    dashboard) use "any message from this device, any reason" as the
//    liveness signal, so no dashboard change is needed to support this -
//    it just starts working once senders add periodic heartbeat sends.
//    ACTION REQUIRED: copy this file into the traffic-light and bridge
//    projects as well.

#pragma once
#include <stdint.h>

enum MsgType : uint8_t {
  MSG_TRAFFIC_LIGHT = 1,
  MSG_TRAIN_CONTROL = 2,
};

// Generic "reason" codes - why this message was sent. Shared across device
// types so one reasonToStr()-style helper can handle all of them.
enum EspNowReason : uint8_t {
  REASON_BOOT = 0,         // device just powered on / reset
  REASON_STATE_CHANGE = 1, // the device's reported state changed
  REASON_HEARTBEAT = 2,    // periodic "I'm still alive", no state change
};

typedef struct {
  uint8_t type; // one of MsgType

  union {
    struct {
      char state;     // 'R', 'Y', or 'G' - current light color
      uint8_t reason; // one of EspNowReason
    } trafficLight;

    struct {
      int8_t  direction; // e.g. -1 / 0 / 1, or degrees - define convention when you build this
      uint8_t speed;      // 0-255
      float   location;   // however you end up encoding position
      uint8_t reason;     // one of EspNowReason (e.g. REASON_BOOT at power-up)
    } trainControl;
  } payload;

} EspNowMessage;
