# ESP32-S3 general-purpose carrier board — design spec v0.1

A reusable, battery-powered ESP32-S3 board for driving LEDs, a button, servos, and a
distance sensor across multiple projects. SMD-first, native USB (no external USB-UART
bridge), low-power battery operation with charging.

## 1. Architecture summary

- **MCU**: ESP32-S3-WROOM-1 module (pre-certified, SMD castellated, native USB)
- **Battery**: single-cell Li-Po/Li-ion, 3.0–4.2V, via JST-PH 2-pin connector
- **Charging**: USB-C in, linear charger + independent battery protection
- **3.3V rail**: buck converter (not LDO) for battery-life efficiency
- **5V rail**: boost converter, GPIO-switchable, powers servos only
- **Programming**: native USB (ESP32-S3 ROM USB-Serial/JTAG) — no CP2102/CH340 needed

## 2. Bill of materials (key components)

| Function | Part | Package | Notes |
|---|---|---|---|
| MCU module | ESP32-S3-WROOM-1-N8R2 (or N16R8) | SMD, castellated | Pick flash/PSRAM size by project needs; N8R2 covers most cases |
| Battery charger | MCP73831T-2ACI/OT (500mA, or -4ACI for other current) | SOT23-5 | Set charge current via R_PROG resistor |
| Battery protection | DW01A + FS8205A | SOT23-6 / SOP-8 | Standard over-charge/discharge/short protection pair used on nearly every LiPo board |
| 3.3V buck converter | TLV62569DBVR or RT6150B-33GQW | SOT23-6 / QFN | ~1A capable, ~20–30µA quiescent, good efficiency across WiFi TX load range |
| 5V boost converter | TPS61088 (higher current) or MT3608 (budget) | QFN / SOT23-6 | EN pin wired to a GPIO for on/off switching |
| USB-C connector | USB4085/USB4105 or similar 16-pin SMD | SMD | Include 5.1kΩ CC1/CC2 pull-downs (required for device-mode power negotiation) |
| USB ESD protection | USBLC6-2SC6 | SOT23-6 | Sits on D+/D- lines between connector and ESP32-S3 |
| Battery connector | JST-PH 2-pin, right-angle or vertical | THT (JST PH is THT; SMD variants exist but less common) | 2.0mm pitch, standard LiPo pack connector |
| I/O connectors (LEDs, button, sensor) | JST-PH 2-pin ×4, JST-PH 4-pin ×1 | THT | See pinout tables below |
| Servo connectors | JST-PH 3-pin ×2 | THT | See pinout table — confirm polarity against your specific servo cables |
| Status LED (charge) | 0603 LED + resistor | SMD | Driven by charger STAT pin |
| Power switch | SPDT slide switch | THT/SMD | In series with battery-to-load path only (not the charge path) |
| Passives | 0603 resistors/caps throughout | SMD | Keeps board SMD-first per your requirement |

Note: JST-PH connectors are inherently through-hole (the housings aren't available in
SMD), so "SMD as possible" applies fully to the ICs and passives — the connectors will
be THT by nature of the JST-PH family.

## 3. GPIO allocation (ESP32-S3-WROOM-1, 44 GPIO available)

| Signal | GPIO (suggested) | Notes |
|---|---|---|
| USB D+/D- | GPIO19/20 | Fixed — native USB pins, don't reassign |
| LED 1/2/3 | GPIO4, 5, 6 | Any general GPIO; on-board series resistor per LED |
| Push button | GPIO7 | Internal pull-up enabled in firmware |
| Servo 1 PWM | GPIO15 | LEDC peripheral |
| Servo 2 PWM | GPIO16 | LEDC peripheral |
| Distance sensor SDA/SCL (I2C mode) | GPIO8, 9 | 4.7kΩ pull-ups to 3.3V on-board |
| Distance sensor Trig/Echo (ultrasonic mode) | reuses GPIO8, 9 | Same header, jumper-selectable power (see §4.4) |
| Boost converter EN | GPIO17 | Drive high to power servo rail, low to save power |
| Battery voltage sense | GPIO1 (ADC) | Resistor divider from BAT net |
| Charger STAT (optional read) | GPIO2 | Optional — lets firmware detect charging state |
| EN / BOOT buttons | EN, GPIO0 | Standard manual reset/flash-mode buttons |
| Spare GPIOs | ~28 remaining | Break out to a 2.54mm pin header for future project-specific expansion |

## 4. Connector pinouts

### 4.1 LED headers (×3, JST-PH 2-pin each)
| Pin | Signal |
|---|---|
| 1 | GPIO (series resistor on-board) |
| 2 | GND |

### 4.2 Push button header (JST-PH 2-pin)
| Pin | Signal |
|---|---|
| 1 | GPIO (internal pull-up) |
| 2 | GND |

### 4.3 Servo headers (×2, JST-PH 3-pin)
| Pin | Signal |
|---|---|
| 1 | GND |
| 2 | 5V (switched boost rail) |
| 3 | PWM signal |

**Check this against your actual servo cables before finalizing the footprint** — servo
cable pin order varies by manufacturer (some are Signal-VCC-GND, others GND-VCC-Signal).
Getting this backwards is the single most common servo-wiring mistake.

### 4.4 Distance sensor header (JST-PH 4-pin, dual-purpose)
| Pin | Ultrasonic mode (e.g. HC-SR04) | I2C ToF mode (e.g. VL53L0X) |
|---|---|---|
| 1 | VCC (5V, via solder-jumper select) | VCC (3.3V, via solder-jumper select) |
| 2 | GND | GND |
| 3 | Trig | SDA |
| 4 | Echo | SCL |

A solder-jumper (or 0Ω resistor pad) on pin 1 selects 3.3V vs 5V for the sensor supply.
Pull-ups on pins 3/4 are always populated (harmless for ultrasonic mode, required for I2C mode).

## 5. Open design notes for the next pass

- Confirm ESP32-S3 module variant (flash/PSRAM size) based on your typical project needs
- Confirm battery capacity/form factor (drives connector orientation and board outline)
- Decide whether to expose the spare ~28 GPIOs as a full header or a curated subset
- Decide charge current (sets the MCP73831 R_PROG resistor value) based on battery capacity
