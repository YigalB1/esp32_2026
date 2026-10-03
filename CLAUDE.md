# esp32_2026 — reusable ESP32-S3 carrier board

## Project goal

A general-purpose, battery-powered ESP32-S3 carrier board, designed for reuse across
several future projects rather than being single-purpose. SMD-first, native USB
(no external USB-UART bridge chip).

Full technical spec — BOM, power tree, GPIO allocation, connector pinouts — lives in
`docs/ESP32-S3_Carrier_Board_Spec.md`. Read that file before doing any schematic work;
it is the source of truth for component choices and pin assignments.

Feature set: 3 LEDs, 1 push button, up to 4 servos, 2 external DC motor driver
interfaces (driver IC deliberately kept off-board — this PCB exposes logic-level
control signals only), up to 4 distance sensors (flexible ultrasonic/I2C-ToF
connectors), a speaker (I2S out) and microphone (I2S in) option. Not every project
using this board will populate every connector — that's expected.

## Tooling: EasyEDA Pro via MCP bridge

Schematic capture happens in EasyEDA Pro, driven through the `easyeda-mcp-pro` MCP
server (configured in `.vscode/mcp.json`). Before relying on it in a session, confirm
the bridge is actually live:

1. Call `easyeda_health_check` and `easyeda_bridge_status` first, every session.
2. If either fails: EasyEDA Pro needs to be open, the bridge extension imported
   (Settings → Extensions → Extension Manager), "Allow External Interaction" enabled,
   and **MCP Bridge → Connect** clicked in EasyEDA Pro's menu bar.

**Important caveat**: the schematic *write* APIs (`easyeda_schematic_place_component`,
`easyeda_schematic_add_wire`, etc.) sit on EasyEDA Pro's beta extension API. Failures
here may be an EasyEDA Pro version limitation, not a bridge misconfiguration — check
`easyeda_get_capabilities` if a write call errors out unexpectedly.

## Suggested workflow

Don't place and wire the whole board in one shot — with ~30 nets across power,
MCU, and I/O sections, that's exactly where AI-driven schematic edits tend to
silently drop connections. Work in stages, verifying after each:

1. Place the ESP32-S3-WROOM-1 module
2. Power section: USB-C → charger/protection → battery → buck (3.3V) → boost (5V, switched)
3. Core I/O: LEDs, button
4. Servo headers (×4)
5. Motor driver logic headers (×2)
6. Distance sensor headers (×4)
7. Audio: mic (on-board) + speaker header

After each stage, call `easyeda_schematic_nets` and `easyeda_erc_run` to confirm
what was actually wired before moving to the next stage — don't trust a "success"
message alone.

Once wiring is verified end-to-end: run `easyeda_drc_run`, generate the BOM
(`easyeda_bom_generate`, cross-check with `easyeda_bom_validate`), and export
Gerbers (`easyeda_export_gerbers`).

## Open decisions still to finalize (see spec doc §6 for detail)

- Exact GPIO pin numbers, checked against WROOM-1 strapping-pin restrictions
- Final battery capacity (revisit upward given peak current from 4 servos + motor logic + audio)
- Whether to keep the shared motor-driver standby/enable line

---

## Train control system (ESP-NOW network) - separate from the carrier board above

Everything above describes the ESP32-S3 carrier board (PCB work). This section is the
train-control software that lives in the same repo. Treat them as two projects that
happen to share the folder.

### Repo layout
- `projects/shared/` - `EspNowProtocol.h`, `EspNowTiming.h`. ONE copy, used by every project.
- `projects/esp_bridge/` - USB bridge (classic ESP32 devkit): ESP-NOW <-> JSON lines over serial.
- `projects/Train_ctrl_esp32_dev/` - single-motor train controller (ESP32-DOIT devkit + DRV8871).
- `projects/Train_ctrl_c3/`, `Ramzor_wemos/`, `esp32_c3_tof/`, `esp_eye/` - other devices.
  Do not modify these unless asked. They only pick up shared-header changes on their next build.
- `tools/train_dashboard.py` - PC dashboard (Python, Tkinter, MQTT).
  `tools/pio_project_launcher.py` - build/upload launcher.
- `pcb/`, `docs/` - hardware design files.

### Conventions that must be preserved
- Source file is `<project_name>_main.cpp`, never `main.cpp`.
- Shared headers are included by relative path: `#include "../../shared/EspNowProtocol.h"`.
  No `-I` build flags, no symlinks, no per-project copies.
- ArduinoJson v7: use `JsonDocument` (`StaticJsonDocument` is deprecated).
- PlatformIO, one `platformio.ini` per project. Build with `pio run` inside the project folder.
  Build every project you touch before saying you are done.
- Never use `delay()` in a device's main loop. Heartbeat and watchdog must not starve - use `millis()`.

### How data flows
- Device -> ESP-NOW -> bridge -> one JSON line per message over USB serial (115200) ->
  dashboard listener thread -> MQTT -> dashboard GUI.
- Dashboard -> one JSON line over the same serial port -> bridge -> ESP-NOW -> device
  (today only `MSG_TRAIN_COMMAND`).
- Bridge MAC `CC:DB:A7:69:97:DC` is the target every device sends to.

### Bridge rules (esp_bridge_main.cpp)
- Only `loop()` writes to Serial. The ESP-NOW receive callback runs on the WiFi task, so it
  must ONLY copy the packet into the FreeRTOS queue. Printing from the callback interleaves
  with loop() output mid-line and corrupts messages (this bug already happened once).
- Every output line is built in full and written with a single call (`emitLine` / `emitJson`).
- A new device must be added to `knownDevices[]` (MAC -> friendly name). All entries are
  registered as ESP-NOW peers at boot.
- Messages with an `"event"` key (boot, ready, error, command_sent, rx_dropped) are bridge
  events, NOT device messages. The dashboard must never treat them as device liveness.

### Protocol rules (EspNowProtocol.h)
- Every message starts with a 1-byte type. A new message = new `MsgType` value + a new branch
  in the payload union. Only add; never change or reorder existing structs or values.
- `EspNowReason`: `REASON_BOOT`, `REASON_STATE_CHANGE`, `REASON_HEARTBEAT` - reuse as is.
- Heartbeat period is `HEARTBEAT_INTERVAL_MS` in `EspNowTiming.h` (20 s).
- Add a changelog entry at the top of the header for every change.

### Dashboard rules (train_dashboard.py)
- Liveness is judged from the dashboard's own wall clock since a device's last message
  (alive / maybe / not connected, thresholds are constants in the dashboard only).
- Devices are grouped by name through `DEVICE_SLOTS`. Train controllers share one slot.
- The watchdog policy belongs to the dashboard (`watchdog_seconds`, default 20 min); the
  board enforces it. The dashboard sends a keepalive every 2 s once the user has acted.
- GUI updates only from the Tk thread; the listener thread talks to it through queues.

### Train_ctrl_esp32_dev board facts
- ESP32-DOIT-DEVKIT-V1. DRV8871: M1_IN1 = GPIO19, M1_IN2 = GPIO23 (PWM, fast decay).
- LEDs are ACTIVE-LOW (LOW = on): LED3 = GPIO13 stopped/red, LED4 = GPIO25 forward/green,
  LED5 = GPIO26 backward/yellow.
- Direction: -1 backward, 0 stop, 1 forward. Modes: AUTO (self-test ramp loop) and MANUAL.
- GPIO34/35 are input-only; do not use them for outputs.
- DS18B20 temperature sensor: GPIO32 (D32), mounted under the motor driver to measure its heat.

### CURRENT TASK (branch temp-sensor): show the DS18B20 temperature on the dashboard
Do it in this order, building and checking each step before the next:

1. **Protocol** - add `MSG_TEMPERATURE = 6` and a `temperature` payload branch:
   `int16_t tempCentiC` (hundredths of a degree C), `uint8_t valid` (0 = sensor missing or
   read failed), `uint8_t reason` (EspNowReason). Add the changelog entry.
2. **Firmware (Train_ctrl_esp32_dev)** - add the OneWire + DallasTemperature libraries to
   `platformio.ini`. Read the sensor on GPIO32 WITHOUT blocking: request a conversion, collect
   the result on a later loop pass (a 12-bit conversion takes up to 750 ms; blocking would stall
   the watchdog and motor loop). Send `MSG_TEMPERATURE` with `REASON_BOOT` at start, with
   `REASON_STATE_CHANGE` when the value moves by 0.5 C or more, and with `REASON_HEARTBEAT` on
   the normal heartbeat timer. A missing or disconnected sensor reports `valid = 0`, never a
   made-up number. Update `DESIGN.md` for the D32 pin.
3. **Bridge** - decode it into
   `{"device":..., "time_s":..., "type":"temperature", "temp_c":<float>, "valid":<bool>, "reason":...}`
   using the existing emit helpers. Keep the single-writer rule.
4. **Dashboard** - handle `type == "temperature"` in the listener and show it in the Train
   Control panel (one decimal, degrees C, `--` when invalid or never received).
   Do not add alarm thresholds yet - ask first.

Hardware check, not code: the DQ line needs a 4.7 k pull-up to 3.3 V and the sensor must be
powered from 3.3 V (GPIO32 is not 5 V tolerant). Remind the user to verify this before testing.

After the change: the bridge and the train board must be reflashed, and the dashboard restarted.
Other devices need no change. Commit to the `temp-sensor` branch only - never to `main`.
