# E-Ink Display Project

A low-power e-ink display system built for the Seeed XIAO ESP32-C3 microcontroller with a GDEM029C90 3-color (black/white/red) e-ink display. The device cycles through various information modules, displaying room temperature/humidity, earthquake data, cat facts, ISS location, and other fun facts.

## Features

### Display Modules

The device cycles through multiple display modules, each showing different information:

- **Room Data**: Temperature and humidity from an SHT31 sensor (no WiFi required)
- **Earthquakes**: Recent earthquake information
- **Meow Facts**: Random cat facts
- **ISS Location**: International Space Station tracking data
- **Useless Facts**: Random trivia facts

### Power Management

- Deep sleep between display cycles to minimize power consumption
- WiFi is only enabled when needed for API calls
- I2C and SPI buses are disabled when not in use
- Battery voltage monitoring and display

### Device Configuration

Each device can fetch its configuration from a remote server:
- Device-specific configs: `https://ota.denton.works/config/devices/{name}.json`
- Fallback to: `https://ota.denton.works/config/devices/default.json`

Configuration options include:
- Display refresh interval
- Module enable/disable toggles
- Custom message endpoints
- Per-device keys/secrets

## Over-The-Air (OTA) Updates

The project includes a comprehensive HTTPS-based OTA update system that allows remote firmware updates without physical access to the device.

### OTA Architecture

#### Version Management

- **Compile-time version**: Defined in `ota_manager.h` as `FIRMWARE_VERSION` (default: "1.0.1")
- **Runtime version**: Stored in ESP32 NVS (Non-Volatile Storage) after successful OTA updates
- **Version comparison**: Semantic versioning (x.y.z format) with automatic comparison
- **Version persistence**: After an OTA update, the new version is stored in NVS and persists across reboots

#### Update Process

1. **Manifest Check**: Device checks a manifest JSON file at startup (when WiFi is connected)
   - Default URL: `https://ota.denton.works/xiao_test/manifest.json`
   - Configurable via `OTA_MANIFEST_URL` in `ota_manager.h`

2. **Version Comparison**: 
   - Compares server version from manifest with current device version
   - Uses NVS-stored version if available (from previous OTA), otherwise uses compiled version
   - Only proceeds if server version is newer

3. **Secure Download**:
   - Uses HTTPS with certificate validation (Root CA certificate required)
   - Optional password protection via `X-OTA-Password` header
   - Downloads firmware in chunks with progress reporting
   - Validates firmware size and integrity

4. **Firmware Installation**:
   - Writes to OTA partition (dual-bank partition scheme)
   - Sets boot partition to new firmware
   - Stores new version in NVS before reboot
   - Automatically reboots to new firmware

#### OTA Configuration

Configure OTA settings in `ota_manager.h`:

```cpp
// Manifest URL (JSON with version and firmware URL)
#define OTA_MANIFEST_URL "https://ota.denton.works/xiao_test/manifest.json"

// Optional password for OTA endpoints
#define OTA_PASSWORD ""

// Current firmware version (keep in sync with version.txt)
#define FIRMWARE_VERSION "1.0.1"

// Debug output (1 = verbose, 0 = minimal)
#define OTA_DEBUG 1
```

#### Root CA Certificate

The OTA system requires a Root CA certificate for HTTPS validation. Store your certificate in `include/root_ca.h` as:

```cpp
const char ROOT_CA[] = R"(
-----BEGIN CERTIFICATE-----
...
-----END CERTIFICATE-----
)";
```

#### Manifest Format

The manifest JSON file should contain:

```json
{
  "version": "1.0.2",
  "sha256": "abc123...",
  "url": "https://ota.denton.works/xiao_test/firmware.bin"
}
```

- `version`: Semantic version string (x.y.z)
- `sha256`: SHA256 hash of the firmware binary (for verification)
- `url`: HTTPS URL to the firmware binary file

### OTA Deployment Workflow

The project includes scripts to automate OTA deployment:

#### 1. Build Firmware

```bash
pio run
```

#### 2. Post-Build Processing (`scripts/post_build.py`)

- Copies compiled firmware from build directory to `ota/firmware.bin`
- Reads version from `version.txt`

#### 3. Create Manifest (`scripts/make_manifest.py`)

- Computes SHA256 hash of firmware binary
- Creates `ota/manifest.json` with version, hash, and firmware URL
- Reads version from `version.txt`

#### 4. Deploy (`scripts/deploy_ota.sh`)

Automated deployment script that:
- Builds the firmware
- Runs post-build processing
- Creates manifest
- Uploads both `firmware.bin` and `manifest.json` to OTA server via SCP

```bash
./scripts/deploy_ota.sh
```

Configuration in `deploy_ota.sh`:
- `OTA_HOST`: SSH host for OTA server (default: `adenton@10.0.0.2`)
- `OTA_PATH`: Remote path on server (default: `/srv/ota/xiao_test`)

### OTA API

The `OTAManager` class provides the following interface:

```cpp
// Initialize OTA manager
otaManager.begin();

// Set configuration
otaManager.setVersionCheckUrl(const char* url);
otaManager.setRootCA(const char* rootCA);
otaManager.setPassword(const char* password);
otaManager.setCurrentVersion(const char* version);
otaManager.setFirmwareUrl(const char* url);  // Direct URL (skips manifest)

// Check for updates
if (otaManager.checkForUpdate()) {
    otaManager.performUpdate();  // Downloads, flashes, reboots
}

// Get current version
const char* version = otaManager.getCurrentVersion();

// Test firmware URL accessibility (no flash)
bool accessible = otaManager.testFirmwareUrl();

// Check update status
bool updating = otaManager.isUpdating();
```

### OTA Partition Scheme

The project uses a dual-bank OTA partition scheme (`partitions.csv`):

- `ota_0`: First OTA partition (0x10000-0x150000)
- `ota_1`: Second OTA partition (0x150000-0x290000)
- `otadata`: OTA data partition for tracking active partition

This allows safe updates: firmware is written to the inactive partition, then the boot partition is switched. If the new firmware fails, the device can roll back to the previous version.

### OTA Debugging

Enable verbose OTA logging by setting `OTA_DEBUG 1` in `ota_manager.h`. This provides detailed information about:
- Manifest fetching and parsing
- Version comparison results
- Download progress (percentage and bytes)
- Firmware write operations
- Partition operations

## Hardware

- **Microcontroller**: Seeed XIAO ESP32-C3
- **Display**: GDEM029C90 3-color e-ink (128x296 pixels, black/white/red)
- **Sensor**: Adafruit SHT31 temperature/humidity sensor
- **Connectivity**: WiFi (ESP32-C3 built-in)

### Pin Configuration

- **SPI**: SCK=21, MOSI=7, MISO=-1, CS=4, DC=5, RST=6, BUSY=3
- **I2C**: SDA=8, SCL=9 (for SHT31 sensor)
- **Battery Monitoring**: ADC pin configured for voltage reading

## Building and Flashing

### Prerequisites

- PlatformIO IDE or CLI
- Python 3 (for build scripts)

### Build

```bash
pio run
```

### Upload

```bash
pio run --target upload
```

### Monitor Serial Output

```bash
pio device monitor
```

## Project Structure

```
e-ink_code/
├── src/                    # Source files
│   ├── ota_manager.cpp/h  # OTA update system
│   └── ota_test.cpp       # OTA testing sketch
├── src_hold/              # Main application code (legacy)
│   ├── main.cpp/h         # Main application
│   ├── api_client.cpp/h   # API client for external data
│   ├── device_config.cpp/h # Device configuration parser
│   ├── device_id.cpp/h    # Device identification
│   └── display_manager.cpp/h # E-ink display functions
├── config/                # Configuration examples
│   └── examples/          # Device config JSON examples
├── scripts/               # Build and deployment scripts
│   ├── deploy_ota.sh     # OTA deployment automation
│   ├── make_manifest.py  # Manifest generation
│   └── post_build.py     # Post-build processing
├── include/               # Header files
│   └── root_ca.h         # Root CA certificate
├── ota/                   # OTA artifacts (generated)
│   ├── firmware.bin       # Compiled firmware for OTA
│   └── manifest.json      # OTA manifest
├── platformio.ini         # PlatformIO configuration
├── partitions.csv         # ESP32 partition table
└── version.txt            # Current firmware version
```

## Configuration

### WiFi Credentials

Set WiFi credentials in your main application code or via build flags.

### Device Configuration

See `config/examples/README.md` for device configuration JSON schema and examples.

## Version Management

- Update `version.txt` with the new version number (e.g., "1.0.2")
- Update `FIRMWARE_VERSION` in `ota_manager.h` to match
- The version is used for OTA update checks and is stored in NVS after successful updates

## License

[Add your license here]

## Contributing

[Add contribution guidelines here]
