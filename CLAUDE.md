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
