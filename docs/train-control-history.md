# Train control system - history, decisions and open items

Captured from the long working chat with Claude (Sept - Oct 2026). The CODE is the source of
truth for how things work; this file records WHY things are the way they are, what version
each part is at, the traps we hit, and what is still open. Anything marked TBD is not decided.
When you finish a feature, add an entry here (see "How to keep this up to date" at the end).

## 1. The system in one picture

    device (ESP32 / Wemos)  --ESP-NOW-->  bridge (ESP32 on USB)  --JSON lines, 115200-->  dashboard (PC)
    device                  <--ESP-NOW--  bridge                 <--JSON lines-----------  dashboard

- Devices send small binary messages to the bridge. The bridge turns each into one JSON line.
- The dashboard (Python + Tkinter + MQTT) shows them and sends commands back the same way.
- Today the only message that flows DOWN is `MSG_TRAIN_COMMAND`.

## 2. Parts and where they live

| Part | Folder | Covered in this chat? |
|---|---|---|
| Shared protocol + timing | `projects/shared/` (`EspNowProtocol.h`, `EspNowTiming.h`) | yes |
| Bridge | `projects/esp_bridge/` | yes |
| Train board, ESP32 dev board (DRV8871) | `projects/Train_ctrl_esp32_dev/` (has `DESIGN.md`) | yes |
| 3D-printed case for that board | `projects/Train_ctrl_esp32_dev/openscad/` | yes |
| Dashboard + PlatformIO launcher | `tools/` | yes |
| Train board, ESP32-C3 | `projects/Train_ctrl_c3/` | no |
| Ramzor traffic lights (Wemos) | `projects/Ramzor_wemos/` | no |
| Distance sensors (VL53L0X) | `projects/esp32_c3_tof/` | no |
| Camera detection | `projects/esp_eye/` | no |
| Carrier board PCB (separate project) | `docs/`, `pcb/` | no |

## 3. Protocol (projects/shared/EspNowProtocol.h)

Every message starts with a 1-byte type. Only ever ADD types; never change existing ones.

| Id | Type | Direction |
|---|---|---|
| 1 | `MSG_TRAFFIC_LIGHT` | device -> bridge |
| 2 | `MSG_TRAIN_CONTROL` | device -> bridge (direction, speed, location, reason) |
| 3 | `MSG_TRAIN_COMMAND` | dashboard -> bridge -> device (mode, running, direction, speed, watchdogSeconds) |
| 4 | `MSG_CAMERA_DETECT` | camera -> bridge |
| 5 | `MSG_DISTANCE_SENSOR` | distance device -> bridge |
| 6 | `MSG_TEMPERATURE` | train board -> bridge (tempCentiC int16, valid, reason) |

Reasons: boot, state_change, heartbeat. Heartbeat period: `HEARTBEAT_INTERVAL_MS` = 20 s
(`EspNowTiming.h`). Direction convention: -1 backward, 0 stop, 1 forward.

## 4. Train_ctrl_esp32_dev (ESP32-DOIT-DEVKIT-V1 + Adafruit DRV8871)

- Motor: M1_IN1 = GPIO19, M1_IN2 = GPIO23, PWM 20 kHz, 8 bit. Stop = both inputs high (brake).
- Status LEDs are ACTIVE-LOW (LOW = on): GPIO13 stopped/red, GPIO25 forward/green,
  GPIO26 backward/yellow (firmware names LED3/4/5; silkscreen on the PCB says LED1/2/3).
- GPIO34/35 are input-only - never use them as outputs.
- DS18B20 temperature sensor on GPIO32, mounted under the motor driver. It needs a 4.7 k pull-up
  (R25) to 3.3 V. R25 was NOT assembled at first - the sensor read "invalid" until it was soldered.
- Boot: send one `REASON_BOOT`, flash all LEDs 2 s, enter AUTO.
- AUTO = self-test loop: forward ramp 0-100 % over 30 s, brake 2 s, backward ramp 30 s, brake 2 s.
- MANUAL = drive exactly what the dashboard last commanded.
- The main loop is a non-blocking `millis()` state machine. NEVER use `delay()` in `loop()`:
  heartbeat and watchdog must not starve.
- Watchdog: the dashboard owns the POLICY (`watchdogSeconds`, default 20 minutes, sent with every
  command); the device enforces it in BOTH modes, because it is the only thing still running if the
  link dies. When tripped: motor stops, self-test pauses until a new command arrives.
- Every command is confirmed back at once with `REASON_STATE_CHANGE`.
- Temperature: a low-priority FreeRTOS task (priority 1, 4096 bytes, no core pinning) requests a
  conversion about every 5 s and posts the result to a length-1 queue (`xQueueOverwrite`). The task
  never touches the radio or Serial. `loop()` sends: `REASON_BOOT` with the first reading (or
  `valid=0` after 5 s), `REASON_STATE_CHANGE` when the value moves >= 0.5 C from the last SENT
  value or validity flips, `REASON_HEARTBEAT` with the latest sample. -127 and exactly 85.0 C are
  treated as invalid (missing sensor / power-on default).
- Libraries: ArduinoJson v7, OneWire, DallasTemperature.

## 5. Bridge (projects/esp_bridge)

- Classic ESP32 devkit on USB, 115200. Known devices are in `knownDevices[]` (MAC -> name) and ALL
  are registered as ESP-NOW peers at boot (needed to be able to send to them).
- Bridge MAC `CC:DB:A7:69:97:DC`. Devices: `train_ctrl_c3` 84:FC:E6:FD:3A:24, `ramzor`
  5C:CF:7F:9A:E9:AB, `ramzor2` 50:02:91:79:3B:F6, `train_ctrl_esp32_dev` CC:DB:A7:6A:19:A8,
  `distance_sensor_1` E8:3D:C1:9D:1B:30, `distance_sensor_2` E8:3D:C1:9D:2F:68.
- RULE: only `loop()` writes to Serial. The ESP-NOW receive callback runs on another task and only
  copies the packet into a FreeRTOS queue. Every output line is built whole and written once.
  (Before this, output from the two tasks interleaved mid-line and real messages were garbled.)
- Serial RX buffer is 2048 bytes (default 256 overflowed on bursts of dashboard commands).
- Events (not device messages): `boot`, `ready`, `error`, `command_sent`, `rx_dropped`.
  `command_sent ok:true` only means "handed to the radio", NOT "the device received it".

## 6. Dashboard (tools/train_dashboard.py)

- Liveness is measured with the dashboard's own clock since a device's last message:
  alive < 30 s, maybe 30-60 s, not connected >= 60 s (`DEVICE_MAYBE_SECONDS`,
  `DEVICE_OFFLINE_SECONDS`). The bridge has its own tighter pair (10 / 20 s).
- Mode buttons: Manual (default panel), Auto, Remote (TBD - does nothing yet).
  Nothing is sent to a device until the user presses a mode/control button.
- Manual panel: Start (green) / Stop (red), speed 0-10 (-> 0-255), direction Left = -1, Right = +1.
- Keepalive: once the user has acted, the current state is re-sent every 2 s in both modes.
- Watchdog field (minutes, default 20, range 1-180) is sent with every command.
- Bridge `command_sent` acks are logged only on failure and must NEVER refresh a device's liveness.
- Speed slider is throttled: sends only when the 0-10 level changes, at most once per 150 ms.
- Train panel shows `Driver temp`: `--` until the first reading, `-- (no sensor reading)` when
  invalid, grey `(old)` when older than 60 s.
- A Ramzor that is not connected (>= 60 s) shows no light instead of its last colour. This is
  display only; the "Controls train" logic still uses the raw last state.
- The listener log opens in its own window. The Ramzor panel is narrow and left-aligned.
- Features built in other chats (not described here): distance sensors, track layout canvas,
  tools panel, per-Ramzor "Controls train" checkboxes, sensor CSV logging.
- The "Commit and Push" button runs `git add -A` from the `esp32_2026` folder and pushes the
  CURRENT branch.

## 7. The 3D-printed case (projects/Train_ctrl_esp32_dev/openscad/)

Current file: `train_ctrl_esp32_dev_case_v8.6.scad`. Box declared locked and working at v8.6.
`PART = "case"` prints box + lid + LED strip; `"led_strip"` only the strip; `"usb_hole_test"` a
round-hole test plate. Every file carries a `VERSION` (printed to the console, engraved under the
floor) and a changelog at the top. Old versions live in git history.

Key facts: board 120 x 94 x 1.6 mm; box 126 x 116 x 51 mm; walls 2 mm; PCB on 6 standoffs
(10 mm tall, 7 mm wide) with M2 heat-set inserts (3.2 mm hole, 4 mm insert); 4 separate lid-screw
bosses in the extra 8 mm of length at each end; lid sits INSIDE the opening with a 45-degree bevel;
2 desk-mounting ears (one per side wall); DC jack holes are 9 x 12 mm rectangles (+0.4 mm),
jacks stick 3 mm out of the PCB edge; USB opening 12 x 9 mm for a USB-C plug; 30 mm voltmeter
hole in the lid; ventilation holes (6 front/back, 2 sides, 6 mm) in the walls only;
three 8 mm LED holes in the lid held by ONE printed retainer strip with 4 glue pads.

| Version | What changed |
|---|---|
| v1-v2 | first drafts (superseded). The stepped corner post of v2-v4 could NOT be assembled |
| v3 | 8 mm LED holes, desk ears, inset bevelled lid |
| v4 | version number + engraving, part colours |
| v5 | lid screws moved to separate bosses in a longer box; 6 plain standoffs; 2 ears |
| v6 | DXF dropped (plain numbers); standoffs 10 mm tall / 7 mm wide |
| v7 | M2 heat-set insert holes |
| v8.0-8.1 | voltmeter hole, hole test plate, DC holes rectangular (round was a misreading) |
| v8.2 | test-fit corrections; `FLIP_Y_FEATURES` (DXF mirror, see gotchas) |
| v8.3 | extra M2 shift; box +20 mm for the voltmeter |
| v8.4 | rectangular USB opening, ventilation holes |
| v8.5 | LED retainer strip |
| v8.6 | 4 glue pads on the strip |

Printing: lid face-down, strip flat with the pads up, no supports. LEDs go in from below, strip
slides in sideways under the flanges, glue only the strip pads to the lid (gel CA on PLA; epoxy on
PETG; CA fumes can fog the clear LED domes). Measure your LED flange (`LED_FLANGE_D/T`).

## 8. Decisions and why

- Watchdog policy lives in the dashboard (it is the "brain"), enforcement on the device (it is the
  only thing left running when the link is down).
- Temperature on its own message type, not squeezed into `trainControl` - additive, keeps every
  other device compatible.
- Temperature read in a separate task so the ~750 ms conversion can never stall the motor loop.
- Heartbeat period shared in one header (`EspNowTiming.h`); the dashboard thresholds are
  dashboard-only (no cross-language sharing needed).
- One shared protocol file for all projects, included by relative path
  (`#include "../../shared/EspNowProtocol.h"`) - no copies, no `-I` flags.
- Source file naming: `<project>_main.cpp`, never `main.cpp`.

## 9. Gotchas and lessons (read before debugging)

1. Flashing: unplug the OTHER ESP32 boards from USB first (PlatformIO may pick the wrong COM
   port) and close the dashboard (it holds the bridge's port).
2. Never feed the board 5 V from DC and USB at once unless the devkit has a diode on VIN - it can
   back-feed the PC's USB port.
3. The bridge file in git was OLDER than the firmware actually running (it lacked ramzor2, distance
   sensors, the queue). Always commit exactly the file you flash.
4. Mixed output from two tasks corrupts serial lines - see the bridge rule above.
5. The DXF export of the PCB is a MIRROR IMAGE of the real top view (fitted against a photo:
   determinant -281). Positions taken from it were flipped along the jack wall. Trust measured
   numbers and test-fit prints; do not trust the DXF for anything asymmetric.
6. A DS18B20 reports 85.0 C right after power-up and -127 when missing: both mean "no reading".
7. PowerShell shows UTF-8 files garbled (a dash appears as 3 odd characters). The file is fine.
   Edit .md/.cpp in VS Code; do not append with `Add-Content` or `>>`.
8. "LF will be replaced by CRLF" warnings from git are harmless on Windows.

## 10. Open items (TBD)

- Dashboard: Remote mode (button exists, does nothing); temperature warning thresholds (wait for
  real numbers under load); what an OFFLINE Ramzor should do to the train (today: nothing).
- Temperature: sensor is in free air under the driver, so it under-reads. Plan: sensor on a JST
  cable, thermal paste + Kapton on the driver copper, a SECOND DS18B20 on the same wire for box air,
  read both by ADDRESS (never by index), add a `sensorId` to `MSG_TEMPERATURE`, calibrate against an
  IR thermometer after the box is closed, optionally log temperatures to a file.
- Commands: the bridge cannot confirm delivery (needs an ESP-NOW send callback, queued like receive).
- Case: confirm the two M2 standoffs near the DC wall in the final fit; LEDs sit about 24 mm below
  the lid and need wires or long leads to the PCB; USB is power-only - programming through it
  (data cable, BOOT/EN access) is TBD; on/off switch is external (on the USB supply).
- Carrier board docs still describe an ESP32-S3 while the design moved to the ESP32-C3 Super Mini.
- READMEs still to write for: Ramzor, distance sensors, camera, Train_ctrl_c3 (ask Claude Code to
  write them from the code).

## 11. Restarting from scratch (order matters)

1. Check out the branch/tag you want. Open `esp32_2026` in VS Code.
2. Flash the TRAIN BOARD first (only it on USB; DC rails OFF while on USB).
3. Flash the BRIDGE (only the bridge on USB; dashboard closed).
4. Plug the bridge into the PC, start `tools/train_dashboard.py`, power the train board from DC.
5. Within about 20 s the train row goes alive and `Driver temp` shows a value.
6. Wheels OFF the track for the first test: Auto ramp smooth, Manual Start/Stop/slider respond at once.

## 12. Git

Branches so far: `main`, `manual-control` (manual mode + watchdog + dashboard + case early work),
`temp-sensor` (temperature + case v8.x + dashboard polish). Tag every known-good state, for example
`git tag -a train-v1 -m "..."` then `git push origin train-v1`. To go back: `git switch --detach train-v1`.

## How to keep this up to date

After every finished feature: (1) update that folder's README.md, (2) add a short dated entry
here (what changed, why, version, new open items), (3) commit docs together with the code.
