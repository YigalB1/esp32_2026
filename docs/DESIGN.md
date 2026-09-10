# Train_ctrl_esp32_dev — Design

## Overview
Single-motor train controller board, built around an ESP32-DOIT-DEVKIT-V1
(ESP-WROOM-32). Older board revision (`Train_CTRL_2024-06-11`, rev 1.0),
only partially assembled — see "Active hardware" below for what's actually
populated on this physical unit.

Communicates with the ESP-NOW bridge (`esp_bridge`) using the shared
`EspNowProtocol.h`, same as `train_ctrl_c3`. Unlike `train_ctrl_c3` (dual
motor, TB6612-style driver), this board drives a **single motor** via an
Adafruit DRV8871 H-bridge.

## Active hardware (per schematic `Train_CTRL_2024-06-11_07-31-04`, rev 1.0)
Only the following are physically populated on this board — everything
else on the schematic (servo, DS18B20, HC-SR04, I2C expander, speed pot,
buzzer, second motor header, voltage/current meter, external regulator)
is present in the design but **not assembled** and out of scope.

| Function | Ref | Notes |
|---|---|---|
| MCU | U1 | ESP32-DOIT-DEVKIT-V1 (ESP-WROOM-32) |
| Motor driver | U3 | Adafruit DRV8871, single H-bridge |
| Power input | U6 | DC power jack |
| Status LEDs | LED1/2/3 (silkscreen) → firmware LED3/4/5 | See pin table |

## Pin map

| Signal | GPIO | Notes |
|---|---|---|
| M1_IN1 | D19 | DRV8871 IN1, PWM |
| M1_IN2 | D23 | DRV8871 IN2, PWM |
| LED3 (stopped / Red) | D13 | Output |
| LED4 (forward / Green) | D25 | Output |
| LED5 (backward / Yellow) | D26 | Output |

**Known constraint:** GPIO34/35 are input-only on this ESP32 variant and
cannot drive the original schematic's LED1/LED2 nets — this board's
status LEDs were reassigned to GPIO13/25/26 (LED3/4/5) to avoid them.
Per the schematic's own revision notes, GPIO36 also has a known ESP32
errata issue with interrupt-driven input and should be avoided for that
use — not used by this board's active circuitry, but noted for reference.

## Liveness / heartbeat
- This board sends a `REASON_HEARTBEAT` message every
  `HEARTBEAT_INTERVAL_MS` (currently 20s), reporting its current
  direction/speed, independent of any real state change.
- The interval lives in a new shared header, `EspNowTiming.h`
  (`esp32_2026/projects/shared/`), included the same way as
  `EspNowProtocol.h`. This is the *only* timing value that needs to be
  shared across devices — it must match across every sender board so
  they all beat at the same cadence.
- Dashboard-side liveness thresholds ("maybe" at 30s silence, "not
  connected" at 60s) are **not** in this shared header — they're pure
  dashboard display policy, computed from the dashboard's own wall-clock
  gap since each device's last message, and only need to exist in the
  dashboard script itself.
- Because a heartbeat has to fire on schedule regardless of what the
  self-test is doing, the main loop was rewritten as a non-blocking
  `millis()`-based state machine (`updateSelfTest()` + `checkHeartbeat()`
  called every `loop()` iteration) rather than using `delay()` for the
  ramp/brake steps — a blocking delay could otherwise starve the
  heartbeat schedule.

## Motor control
- Fast-decay drive via DRV8871 IN1/IN2 (PWM carried directly on the input
  pins, same style as `train_ctrl_c3`'s TB6612 driver).
- Forward: PWM on IN1, IN2 low.
- Backward: PWM on IN2, IN1 low.
- Stop: both IN1/IN2 high (DRV8871 fast-decay brake).
- Note: the shared protocol has no `CMD_STOP_BRAKE` / `CMD_STOP_RAMP` /
  `ramp_time_s` fields — those were an incorrect assumption in an earlier
  draft of this doc, carried over from `train_ctrl_c3`'s internal motor
  logic. The actual `trainControl` payload (see Protocol below) only has
  `direction`, `speed`, `location`, `reason` — any ramping is done
  locally by this firmware, not commanded by the protocol.

## LED behavior
Same semantics as `train_ctrl_c3`, renumbered to this board's available
pins:
- LED3 (D13, Red) — stopped
- LED4 (D25, Green) — forward
- LED5 (D26, Yellow) — backward

**Polarity: active-low.** These LEDs turn on when the GPIO is driven
LOW, not HIGH — opposite of the usual assumption. Firmware handles this
centrally in `setAllLeds()` / `flashAllLeds()`, so nothing at the call
sites needs to know about polarity.

## Protocol
- Uses the shared `EspNowProtocol.h` (`esp32_2026/projects/shared/`),
  included via `#include "../../shared/EspNowProtocol.h"`.
- `trainControl` payload actually defined as:
  `direction` (int8_t), `speed` (uint8_t, 0-255), `location` (float),
  `reason` (uint8_t / `EspNowReason`).
- Direction convention for this board: **-1 = backward, 0 = stop,
  1 = forward**.
- `location` has no defined encoding project-wide and this board has no
  position-sensing hardware assembled — left at `0.0f` / unused.
- Note: the struct has only one direction/speed pair total (no per-motor
  fields), which is fine for this single-motor board. How `train_ctrl_c3`
  (dual motor) maps onto this same struct is outside this board's scope
  and not something this doc resolves.
- Boot sequence: same shape as `train_ctrl_c3` — one ESP-NOW `REASON_BOOT`
  announcement, flash all LEDs for 2s, then an endless Motor 1 self-test
  loop (forward ramp 0→100% over 30s, brake/pause 2s, backward ramp
  0→100% over 30s, brake/pause 2s, repeat).

## ESP-NOW peer (bridge)
- Bridge MAC address: `CC:DB:A7:69:97:DC` (same bridge as `train_ctrl_c3`,
  confirmed via its boot log).
- This board prints its own MAC address to Serial at boot, so it can be
  registered as a peer on the bridge side.

## Firmware update
- USB / PlatformIO upload only, at this stage. No OTA (physical access
  to this board is currently easy; may revisit once boxed).
