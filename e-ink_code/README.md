# E-Ink firmware (`e-ink_code`)

PlatformIO firmware for the Seeed **XIAO ESP32-C3** driving a **GDEM029C90-class** 2.9" three-color e-paper display (B/W/R). Pinout and board documentation live in the [repository root README](../README.md).

## Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or IDE)
- Python 3 (OTA helper scripts under `scripts/`)

PlatformIO uses **`src_dir = firmware`** — all application code is under `firmware/`, not `src/`.

## Quick start

```bash
cd e-ink_code

# Build one app (recommended for smaller binaries)
pio run -e seeed_xiao_sensor

pio run -e seeed_xiao_sensor --target upload
pio device monitor
```

## Build environments

| Environment | `APP_*` flag | Typical use |
|-------------|----------------|-------------|
| `seeed_xiao_esp32c3` | *(none — all apps linked)* | Development / multi-app image |
| `seeed_xiao_fun` | `APP_FUN` | Fun / info modules only |
| `seeed_xiao_sensor` | `APP_SENSOR` | Room sensor + optional Nemo |
| `seeed_xiao_shelf` | `APP_SHELF` | Shelf / bin label |
| `seeed_xiao_messages` | `APP_MESSAGES` | Static message list |

Defined in [`platformio.ini`](platformio.ini).

## Source layout

```
e-ink_code/
├── firmware/
│   ├── main.cpp              # Boot, NVS config, app wiring, BLE gating
│   ├── app_manager/          # App selection + JSON configure
│   ├── core/
│   │   ├── hardware_config.h # Pins, battery, OTA URL macros (edit for your server)
│   │   ├── bluetooth/        # Cold-start BLE setup
│   │   ├── display/, wifi/, power/, ota/
│   └── apps/
│       ├── fun/              # Rotating “modules” (sensor + HTTP APIs)
│       ├── sensor/           # SHT31 + optional Nemo posting
│       ├── shelf/            # HTTP client to bin-lookup style server
│       └── messages/         # Up to 10 configured strings
├── scripts/                  # deploy_ota.sh, manifest, flash helpers
├── config/examples/          # JSON shape examples for BLE / tools
├── include/                  # Optional shared headers (e.g. reference CA PEM)
├── partitions.csv
├── version.txt               # Keep in sync with FIRMWARE_VERSION for OTA
└── platformio.ini
```

## Apps (summary)

| App | Role |
|-----|------|
| **fun** | Local SHT31 “room” tile plus optional WiFi feeds (earthquake, cat facts, ISS, trivia); cycles modes; **only this app wires HTTPS OTA** today (`handleOTA()`). |
| **sensor** | Temperature/humidity display; optional Nemo/API posting per `config`. |
| **shelf** | Fetches label text from a configurable HTTP server (`serverHost` / `serverPort` / `binId`). |
| **messages** | Shows a fixed list of lines from config. |

Active app and `config` object come from JSON (BLE-stored NVS or the built-in test default in `main.cpp`). See [`config/examples/README.md`](config/examples/README.md).

## Pins (authoritative: `firmware/core/hardware_config.h`)

| Signal | GPIO |
|--------|------|
| SPI SCK | 3 |
| SPI MOSI | 4 |
| E-ink CS | 7 |
| E-ink DC | 6 |
| E-ink RST | 5 |
| E-ink BUSY | 21 |
| I2C SDA / SCL (SHT31) | 9 / 10 |
| Battery ADC | 2 |
| Display + sensor power MOSFET gate | 8 |
| Divider enable `V_SWITCH` | 20 |

## Bluetooth setup (cold start)

- Implemented in [`firmware/core/bluetooth/cold_start_ble.cpp`](firmware/core/bluetooth/cold_start_ble.cpp).
- BLE runs only on a true cold start (not deep-sleep timer wake); advertising window **15 s** (`COLD_START_BLE_WINDOW_SECONDS` in [`cold_start_ble.h`](firmware/core/bluetooth/cold_start_ble.h)).
- WiFi is turned off while BLE is active (ESP32-C3 coexistence).
- After JSON config is written to the TX characteristic, preferences store `configJson` and set a one-shot **`skipBLE`** so the next reboot applies settings without re-entering BLE.

## Power and deep sleep

- Apps call `PowerManager::enterDeepSleep()` after a refresh cycle.
- Set **`DISABLE_DEEP_SLEEP_FOR_TESTING`** to `1` in [`hardware_config.h`](firmware/core/hardware_config.h) to replace deep sleep with a long `delay()` (USB serial stays usable).

## OTA updates

- **Fun** app: `handleOTA()` passes `OTA_VERSION_CHECK_URL`, `ROOT_CA_CERT`, `OTA_PASSWORD`, and `FIRMWARE_VERSION` from [`hardware_config.h`](firmware/core/hardware_config.h) into `OTAManager`.
- Manifest over HTTPS must be JSON with at least **`version`** and **`url`** (firmware `.bin`). Version must be newer than the device (`x.y.z` compared in [`ota_manager.cpp`](firmware/core/ota/ota_manager.cpp)).
- `scripts/make_manifest.py` also writes **`sha256`** for your deployment records; the current firmware path does not verify that hash on device.
- Dual OTA partitions: [`partitions.csv`](partitions.csv). Deploy flow: `scripts/deploy_ota.sh` (see script for host/path variables).

## Scripts

| Script | Purpose |
|--------|---------|
| `scripts/deploy_ota.sh` | Build, post-build, manifest, SCP to server |
| `scripts/make_manifest.py` | `ota/manifest.json` from `version.txt` + `ota/firmware.bin` |
| `scripts/post_build.py` | Copy firmware binary into `ota/` |
| `scripts/flash_firmware.sh` | USB flash helper |

## Versioning

1. Bump [`version.txt`](version.txt).
2. Match **`FIRMWARE_VERSION`** in [`firmware/core/hardware_config.h`](firmware/core/hardware_config.h).

## License

Firmware, scripts, config examples, and Markdown here are **MIT**. See [`LICENSE-SOFTWARE`](../LICENSE-SOFTWARE) and [`LICENSE`](../LICENSE). Hardware CAD is **CERN-OHL-W-2.0** under [`../e-ink-PCBs/`](../e-ink-PCBs/).
