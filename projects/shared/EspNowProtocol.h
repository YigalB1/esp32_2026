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
//  - Added MSG_TRAIN_COMMAND, ControlMode, and the trainCommand payload -
//    the first message type that flows DOWN (dashboard -> bridge -> a
//    device) rather than up. Lets the dashboard switch a train controller
//    between MODE_AUTO (run its own programmed behavior, e.g. the
//    self-test loop) and MODE_MANUAL (drive exactly what's commanded:
//    running/direction/speed). This does NOT change the size or layout of
//    the existing trainControl struct - it's a new branch in the union.
//    ACTION REQUIRED: copy this file into every project that sends OR
//    receives train commands (train controllers, the bridge).
//  - Added watchdogSeconds to trainCommand. Design split: the DEVICE is
//    the only thing still running once the link is down, so it's the
//    only thing that can actually enforce a cutoff (mechanism) - but the
//    THRESHOLD is decided and owned by the dashboard (policy), sent with
//    every command rather than hardcoded in firmware. Applies in both
//    MODE_AUTO and MODE_MANUAL - the dashboard is expected to keep
//    "checking in" periodically (well under this threshold) in either
//    mode, not just while manually driving.
//    ACTION REQUIRED: copy this file into every project that sends OR
//    receives train commands (train controllers, the bridge).
//  - Added MSG_CAMERA_DETECT and the cameraDetect payload - the first
//    message type from the esp32_eye/ESP32-CAM camera-based detection
//    devices (device -> bridge). Reuses EspNowReason as-is: REASON_BOOT
//    for the boot announcement, REASON_STATE_CHANGE whenever the device's
//    empty/object classification flips, REASON_HEARTBEAT on the same
//    periodic timer as other device types. Deliberately minimal for now -
//    just the current state - since the devices are still desk-testing
//    detection logic. Calibration push (dashboard -> device) and any
//    health/confidence reporting (noise floor, threshold - for flagging a
//    camera as unreliable due to lighting) are expected to be separate
//    future message types once that work starts, not fields bolted onto
//    this one.
//    Since this lives in one shared location and every project includes
//    it by relative path, no copying is needed - just make sure any
//    checked-out copies (if you have more than one clone) pick up this
//    updated file before building esp32_eye or esp_bridge.

#pragma once
#include <stdint.h>

enum MsgType : uint8_t {
  MSG_TRAFFIC_LIGHT = 1,
  MSG_TRAIN_CONTROL = 2,
  MSG_TRAIN_COMMAND = 3, // dashboard -> bridge -> device: switch mode / drive manually
  MSG_CAMERA_DETECT = 4, // camera device -> bridge: empty/object state
};

// Generic "reason" codes - why this message was sent. Shared across device
// types so one reasonToStr()-style helper can handle all of them.
enum EspNowReason : uint8_t {
  REASON_BOOT = 0,         // device just powered on / reset
  REASON_STATE_CHANGE = 1, // the device's reported state changed
  REASON_HEARTBEAT = 2,    // periodic "I'm still alive", no state change
};

// Which behavior a train controller is currently following.
enum ControlMode : uint8_t {
  MODE_AUTO = 0,   // run the device's own programmed behavior (e.g. self-test loop)
  MODE_MANUAL = 1, // drive exactly what's commanded (running/direction/speed below)
};

// Whether a camera-based detection device currently sees the track as
// clear or occupied. Kept separate from EspNowReason - state is "what is
// true right now", reason is "why you're hearing about it this time".
enum DetectState : uint8_t {
  DETECT_EMPTY = 0,
  DETECT_OBJECT = 1,
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

    struct {
      uint8_t mode;             // one of ControlMode
      uint8_t running;          // 0 = stopped, 1 = running - only meaningful in MODE_MANUAL
      int8_t  direction;        // -1/0/1, same convention as trainControl - only meaningful if running
      uint8_t speed;            // 0-255 - only meaningful if running
      uint16_t watchdogSeconds; // dashboard-set: seconds without a command before
                                 // the device fail-safes (stops the motor). Applies
                                 // in both MODE_AUTO and MODE_MANUAL. 0 = leave the
                                 // device's current watchdog value unchanged.
    } trainCommand;

    struct {
      uint8_t state;  // one of DetectState - empty or object, right now
      uint8_t reason; // one of EspNowReason
    } cameraDetect;
  } payload;

} EspNowMessage;
