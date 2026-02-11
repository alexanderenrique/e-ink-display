# E-Ink Display

A custom ESP32-C3-based e-ink display platform with a modular app system, BLE configuration, OTA updates, and two KiCad PCB designs (through-hole and SMD). Built around the Seeed XIAO ESP32-C3 and a 2.9" tri-color (black/white/red) e-ink panel.

## Project Structure

```
e-ink-display/
├── e-ink_code/                  # Firmware and scripts (PlatformIO project)
│   ├── firmware/                # ESP32-C3 firmware source
│   │   ├── main.cpp             # Entry point, app registration, BLE config loading
│   │   ├── core/                # Hardware abstraction and shared services
│   │   │   ├── hardware_config.h
│   │   │   ├── wifi/
│   │   │   ├── display/
│   │   │   ├── power/
│   │   │   ├── bluetooth/
│   │   │   └── ota/
│   │   ├── app_manager/         # Plugin-based app system
│   │   │   ├── app_interface.h
│   │   │   ├── app_manager.h
│   │   │   └── app_manager.cpp
│   │   └── apps/                # Individual apps
│   │       ├── fun/
│   │       ├── sensor/
│   │       ├── shelf/
│   │       └── messages/
│   ├── scripts/                 # Build, deploy, and utility scripts
│   └── platformio.ini           # Build configuration and environments
├── e-ink_PCB_throughhole/       # KiCad PCB design (through-hole components)
├── e-ink_PCB_SMD/               # KiCad PCB design (surface-mount components)
└── README.md
```

## Hardware

### Components

| Component | Description |
|---|---|
| Seeed XIAO ESP32-C3 | Microcontroller with WiFi and BLE |
| GDEM029C90 | 2.9" 128x296 tri-color e-ink display (black/white/red) |
| SHT31 | I2C temperature and humidity sensor |
| FQP27P06 | P-channel MOSFET for power switching |
| 100 uF capacitor | Decoupling |
| Resistors (x3) | Voltage divider (47k/68k) and pull-up |
| Battery connector | LiPo battery input |

### Pin Mapping (ESP32-C3)

| Pin | Function |
|---|---|
| GPIO 21 | SPI SCK (e-ink) |
| GPIO 7 | SPI MOSI (e-ink) |
| GPIO 6 | e-ink RST |
| GPIO 5 | e-ink DC |
| GPIO 4 | e-ink CS |
| GPIO 3 | e-ink BUSY |
| GPIO 9 | I2C SDA (SHT31) |
| GPIO 10 | I2C SCL (SHT31) |
| GPIO 2 | Battery ADC (voltage divider) |
| GPIO 8 | Power MOSFET gate |

### PCB Designs

Two KiCad PCB variants are included:

- **`e-ink_PCB_throughhole/`** -- Through-hole components for easier hand-soldering.
- **`e-ink_PCB_SMD/`** -- Surface-mount components for a smaller footprint.

Both share the same schematic and pin mapping. Each directory contains the KiCad project files (`.kicad_pro`, `.kicad_sch`, `.kicad_pcb`) and exported Gerber files ready for fabrication.

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
|---|---|
| **fun** | Cycles through cat facts, earthquake data, ISS location, useless facts, and local room temperature/humidity. Rotates display mode across deep sleep cycles. |
| **sensor** | Reads temperature and humidity from the SHT31 sensor, displays on screen, and optionally posts data to a Nemo API endpoint. Supports Celsius/Fahrenheit. |
| **shelf** | Queries a bin lookup server to display shelf/bin owner information. Designed for lab inventory labeling. |
| **messages** | Displays a configurable list of up to 10 messages, cycling through them on each wake. |

### BLE Configuration

On cold boot (power-on reset), the device advertises via BLE for up to 3 minutes. A BLE central (phone app, script, etc.) can connect and send a JSON configuration including WiFi credentials, app selection, and app-specific settings. The configuration is stored in NVS (non-volatile storage) and persists across deep sleep and reboots.

### Power Management

- Battery voltage is read through a 47k/68k resistor divider on GPIO 2.
- Deep sleep is used between display refreshes (configurable per app).
- When battery drops below 5%, the device shows a low-battery message and enters a periodic wake-check sleep (every 5 minutes). Normal operation resumes at 15%.

### OTA Updates

The firmware supports HTTPS OTA updates. A deploy script builds the firmware, generates a manifest (version + SHA256), and uploads it to a remote server. Devices check for updates and apply them over WiFi.

### Build Environments

Each app has its own PlatformIO build environment that only compiles the code for that specific app:

| Environment | App | Build flag |
|---|---|---|
| `seeed_xiao_esp32c3` | All apps | (none) |
| `seeed_xiao_fun` | Fun | `-DAPP_FUN` |
| `seeed_xiao_sensor` | Sensor | `-DAPP_SENSOR` |
| `seeed_xiao_shelf` | Shelf | `-DAPP_SHELF` |
| `seeed_xiao_messages` | Messages | `-DAPP_MESSAGES` |

### Dependencies

Managed by PlatformIO (`lib_deps` in `platformio.ini`):

- [GxEPD2](https://github.com/ZinggJM/GxEPD2) -- E-ink display driver
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) ^6.21.3 -- JSON parsing
- [Adafruit SHT31 Library](https://github.com/adafruit/Adafruit_SHT31) ^2.2.2 -- Temperature/humidity sensor
- [Adafruit Unified Sensor](https://github.com/adafruit/Adafruit_Sensor) ^1.1.9 -- Sensor abstraction
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) ^1.4.1 -- BLE stack

## Scripts

Located in `e-ink_code/scripts/`:

| Script | Description |
|---|---|
| `deploy_ota.sh` | Build firmware, create manifest, and SCP to OTA server |
| `flash_firmware.sh` | Flash firmware to ESP32-C3 via USB using esptool |
| `bin_lookup_server.py` | Python HTTP server that proxies NEMO API bin/user data for the shelf app |
| `scan_ble.py` | BLE scanner utility to discover and inspect nearby devices |
| `post_build.py` | Copies built firmware binary to `ota/` directory |
| `make_manifest.py` | Generates `manifest.json` with version and SHA256 hash |

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or IDE plugin)
- [KiCad](https://www.kicad.org/) 8+ (for PCB design files)
- Python 3 (for utility scripts)

### Build and Flash

```bash
cd e-ink_code

# Build a specific app
pio run -e seeed_xiao_sensor

# Upload via USB
pio run -e seeed_xiao_sensor --target upload

# Monitor serial output
pio device monitor
```

### Deploy OTA Update

```bash
cd e-ink_code
./scripts/deploy_ota.sh sensor
```

### Run the Bin Lookup Server (Shelf App)

```bash
cd e-ink_code
pip install requests python-dotenv
# Set NEMO_API_KEY in a .env file
python scripts/bin_lookup_server.py
```
