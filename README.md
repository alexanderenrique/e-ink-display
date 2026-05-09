# E-Ink Display

A custom ESP32-C3-based e-ink display platform with a modular app system, BLE configuration, OTA updates, and three KiCad PCB variants. Built around the Seeed XIAO ESP32-C3 and a 2.9" tri-color (black/white/red) e-ink panel (e.g. GDEM029C90).

## Licensing

Hardware design files in [`e-ink-PCBs/`](e-ink-PCBs/) are licensed under **CERN-OHL-W-2.0**. Firmware, scripts, config examples, and Markdown documentation are under the **MIT License**. See [`LICENSE`](LICENSE), [`LICENSE-HARDWARE`](LICENSE-HARDWARE), and [`LICENSE-SOFTWARE`](LICENSE-SOFTWARE).

## Open source scope (third-party parts)

This project is shared to meet the [Open Source Hardware Definition](https://www.oshwa.org/definition/) and aligns with the spirit of [OSHWA certification requirements](https://certification.oshwa.org/requirements.html) for distinguishing parts outside the licensor’s control.

**Released under the licenses above (our work):**

- Schematics and PCB layouts in [`e-ink-PCBs/`](e-ink-PCBs/) (native KiCad sources and Gerber exports in each variant folder).
- Firmware in [`e-ink_code/firmware/`](e-ink_code/firmware/), plus tooling in [`e-ink_code/scripts/`](e-ink_code/scripts/) and examples under [`e-ink_code/config/`](e-ink_code/config/).

**Not open-sourced by this repository (standard commercial / third-party parts; use vendor documentation):**

| Item | Role | Public datasheet / info |
|------|------|-------------------------|
| Seeed XIAO ESP32-C3 | MCU module (ESP32-C3, crystal, antenna, regulators inside module) | Seeed Wiki and [ESP32-C3 technical reference](https://www.espressif.com/en/products/socs/esp32-c3) |
| GDEM029C90 (or compatible Good Display panel) | E-ink panel + timing/controller as supplied by vendor | Vendor PDF (e.g. Good Display product pages) |
| SHT31 | Temperature / humidity sensor IC or module | [Sensirion SHT31 datasheet](https://www.sensirion.com/) |
| FQP27P06, ZXMP4A16G | Discrete MOSFETs | [ON Semi FQP27P06](https://www.onsemi.com/), [Diodes ZXMP4A16G](https://www.diodes.com/) |
| MCP7940N (RTC variant only) | I2C real-time clock | [Microchip MCP7940N](http://ww1.microchip.com/downloads/en/DeviceDoc/20005010F.pdf) |

Optional firmware features (e.g. the **shelf** app) may call HTTP APIs or a companion script (`bin_lookup_server.py`); the hardware and core firmware remain usable without those services.

## Project structure

```
e-ink-display/
├── e-ink-PCBs/                 # KiCad 9 hardware + Gerbers
│   ├── SMD/                    # “SMD” layout variant
│   ├── Throughole-Basic/       # Through-hole, base design
│   ├── Throughole-RTC/         # Through-hole + MCP7940N RTC (+ optional SD header in sch)
│   └── bom/                    # CSV BOM summaries (regenerate from KiCad after edits)
├── e-ink_code/                 # Firmware and scripts (PlatformIO)
│   ├── firmware/               # ESP32-C3 source, core/, apps/, main.cpp
│   ├── scripts/               # OTA, flash, BLE utilities
│   ├── config/examples/       # Device JSON examples
│   ├── platformio.ini
│   └── requirements.txt       # Python deps for helper scripts
├── docs/
│   └── images/                # Add board photos or renders here (recommended for makers)
├── LICENSE, LICENSE-HARDWARE, LICENSE-SOFTWARE, README.md
```

## Hardware

### Components

| Component | Description |
|-----------|-------------|
| Seeed XIAO ESP32-C3 | Microcontroller with WiFi and BLE |
| GDEM029C90 | 2.9" 128×296 tri-color e-ink display (black/white/red) |
| SHT31 | I2C temperature and humidity sensor |
| FQP27P06 (and variants may add ZXMP4A16G) | P-channel MOSFETs for power switching |
| 100 µF capacitor | Bulk decoupling (where present in schematic) |
| Resistors | Voltage divider (47 kΩ / 68 kΩ), pull-ups, etc. (see BOM CSVs) |
| Battery connector | LiPo / battery input |
| Through-hole RTC add-ons (Throughole-RTC only) | MCP7940N, 32.768 kHz crystal, load caps; optional SD header in schematic |

### Pin mapping (ESP32-C3)

| Pin | Function |
|-----|-----------|
| GPIO 3 | SPI SCK (e-ink) |
| GPIO 4 | SPI MOSI (e-ink) |
| GPIO 7 | e-ink CS |
| GPIO 6 | e-ink DC |
| GPIO 5 | e-ink RST |
| GPIO 21 | e-ink BUSY |
| GPIO 9 | I2C SDA (SHT31) |
| GPIO 10 | I2C SCL (SHT31) |
| GPIO 2 | Battery ADC (voltage divider) |
| GPIO 8 | Display + sensor power MOSFET gate |
| GPIO 20 | Battery divider sense enable (`V_SWITCH`) |

### PCB variants

| Directory | Description |
|-----------|-------------|
| [`e-ink-PCBs/SMD/`](e-ink-PCBs/SMD/) | Single main FQP27P06 switch; mixed THT footprints on some passives as in schematic |
| [`e-ink-PCBs/Throughole-Basic/`](e-ink-PCBs/Throughole-Basic/) | Through-hole friendly; includes ZXMP4A16G + FQP27P06 in schematic |
| [`e-ink-PCBs/Throughole-RTC/`](e-ink-PCBs/Throughole-RTC/) | Basic topology + MCP7940N RTC, crystal, optional `J6` SD block |

Each folder contains `.kicad_pro`, `.kicad_sch`, `.kicad_pcb`, drill files, and `*.gbr` / `*.gbrjob` fab outputs. **Preferred source for modification is KiCad**, not Gerbers alone.

### Replication: BOM and assembly

1. Open the variant you are building and use **Tools → Generate Bill of Materials** in KiCad to export a fresh CSV when anything changes.
2. Human-readable summaries for each variant live in [`e-ink-PCBs/bom/`](e-ink-PCBs/bom/) — see [`e-ink-PCBs/bom/README.md`](e-ink-PCBs/bom/README.md).
3. Order PCBs using the `e-ink_PCB-job.gbrjob` or individual Gerbers in that variant’s folder.
4. Solder power, MCU module, and display connector before first power-on; double-check battery polarity and MOSFET orientation against the silkscreen.
5. **First firmware:** flash over USB with PlatformIO (see **Build and flash** below) or keep a personal `e-ink_code/FLASH_INSTRUCTIONS.md` locally (that filename is gitignored).
6. **Photos / renders:** add PNGs under [`docs/images/`](docs/images/) (e.g. bare PCB, populated board, enclosure) so others can verify the build.

## Firmware

### Architecture

The firmware uses a plugin-based app system. Core managers handle hardware (WiFi, display, power, OTA, BLE) and are injected into apps via dependency injection. Each app implements the `AppInterface` and is registered with the `AppManager`, which coordinates lifecycle and configuration.

```
Main  -->  AppManager  -->  [Fun | Sensor | Shelf | Messages]
                |
           Core Managers
       (WiFi, Display, Power, OTA, BLE)
```

### Apps

| App | Description |
|-----|-------------|
| **fun** | Cycles through cat facts, earthquake data, ISS location, useless facts, and local room temperature/humidity. Rotates display mode across deep sleep cycles. |
| **sensor** | Reads temperature and humidity from the SHT31 sensor, displays on screen, and optionally posts data to a Nemo API endpoint. Supports Celsius/Fahrenheit. |
| **shelf** | Queries a bin lookup server to display shelf/bin owner information. Designed for lab inventory labeling. |
| **messages** | Displays a configurable list of up to 10 messages, cycling through them on each wake. |

### BLE configuration

On cold boot (power-on reset), the device advertises via BLE for up to 3 minutes. A BLE central (phone app, script, etc.) can connect and send a JSON configuration including WiFi credentials, app selection, and app-specific settings. The configuration is stored in NVS (non-volatile storage) and persists across deep sleep and reboots.

### Power management

- Battery voltage is read through a 47k/68k resistor divider on GPIO 2.
- Deep sleep is used between display refreshes (configurable per app).
- When battery drops below 5%, the device shows a low-battery message and enters a periodic wake-check sleep (every 5 minutes). Normal operation resumes at 15%.

### OTA updates

The firmware supports HTTPS OTA updates. A deploy script builds the firmware, generates a manifest (version + SHA256), and uploads it to a remote server. Devices check for updates and apply them over WiFi.

### Build environments

Each app has its own PlatformIO build environment that only compiles the code for that specific app:

| Environment | App | Build flag |
|-------------|-----|------------|
| `seeed_xiao_esp32c3` | All apps | (none) |
| `seeed_xiao_fun` | Fun | `-DAPP_FUN` |
| `seeed_xiao_sensor` | Sensor | `-DAPP_SENSOR` |
| `seeed_xiao_shelf` | Shelf | `-DAPP_SHELF` |
| `seeed_xiao_messages` | Messages | `-DAPP_MESSAGES` |

### Dependencies

Managed by PlatformIO (`lib_deps` in `platformio.ini`):

- [GxEPD2](https://github.com/ZinggJM/GxEPD2) — E-ink display driver
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) ^6.21.3 — JSON parsing
- [Adafruit SHT31 Library](https://github.com/adafruit/Adafruit_SHT31) ^2.2.2 — Temperature/humidity sensor
- [Adafruit Unified Sensor](https://github.com/adafruit/Adafruit_Sensor) ^1.1.9 — Sensor abstraction
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) ^1.4.1 — BLE stack

## Scripts

Located in `e-ink_code/scripts/`:

| Script | Description |
|--------|-------------|
| `deploy_ota.sh` | Build firmware, create manifest, and SCP to OTA server |
| `flash_firmware.sh` | Flash firmware to ESP32-C3 via USB using esptool |
| `bin_lookup_server.py` | Python HTTP server that proxies NEMO API bin/user data for the shelf app |
| `scan_ble.py` | BLE scanner utility to discover and inspect nearby devices |
| `post_build.py` | Copies built firmware binary to `ota/` directory |
| `make_manifest.py` | Generates `manifest.json` with version and SHA256 hash |

## OSHWA certification (administrative next steps)

After this repository reflects the licenses and documentation above, **certification itself is completed on OSHWA’s site**, not in git:

1. Review [Certification requirements](https://certification.oshwa.org/requirements.html) and the [Open Source Hardware checklist](https://www.oshwa.org/resources/open-source-hardware-checklist).
2. Complete the **Certification Mark License Agreement** and submit documentation links for each hardware version you certify.
3. **Register each unique product** that will carry the certification mark, and follow OSHWA’s **mark usage guidelines** (do not imply endorsement by Seeed, Sensirion, Microchip, OSHWA, etc., beyond factual compatibility).
4. Plan for **annual renewal** emails from OSHWA to reaffirm compliance.

Silkscreen and marketing should use only trademarks you are entitled to; third-party logos on the PCB must be cleared separately from this project’s open-license grants.

## Getting started

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or IDE plugin)
- [KiCad](https://www.kicad.org/) 9+ (matches current `.kicad_*` schema)
- Python 3 (for utility scripts)

### Build and flash

```bash
cd e-ink_code

# Build a specific app
pio run -e seeed_xiao_sensor

# Upload via USB
pio run -e seeed_xiao_sensor --target upload

# Monitor serial output
pio device monitor
```

### Deploy OTA update

```bash
cd e-ink_code
./scripts/deploy_ota.sh sensor
```

### Run the bin lookup server (shelf app)

```bash
cd e-ink_code
pip install requests python-dotenv
# Set NEMO_API_KEY in a .env file
python scripts/bin_lookup_server.py
```
