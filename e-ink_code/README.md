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

#### Deep Sleep Implementation

Deep sleep functionality is implemented but **currently disabled for testing**. Each app handles its own sleep cycle:

**Current State (Testing)**:
- All apps use `delay()` for timing between display cycles
- Deep sleep code is present but commented out in each app's `loop()` method
- This allows continuous operation for testing and debugging

**Deep Sleep Code Location**:
- **Fun App**: `firmware/apps/fun/app.cpp` (lines 202-205)
- **Sensor App**: `firmware/apps/sensor/app.cpp` (lines 150-153)
- **Shelf App**: `firmware/apps/shelf/app.cpp` (lines 55-58)

**Implementation Details**:
- Deep sleep is implemented in `PowerManager::enterDeepSleep()` (`firmware/core/power/power_manager.cpp`)
- Uses ESP32's `esp_sleep_enable_timer_wakeup()` and `esp_deep_sleep_start()`
- Sleep duration is controlled by `refreshIntervalMinutes` from BLE configuration
- Wakeup cause detection is handled in `main.cpp` `setup()` to distinguish cold starts from deep sleep wakeups

**To Enable Deep Sleep**:
1. Uncomment the deep sleep code block in the desired app's `loop()` method
2. The code uses `_power->enterDeepSleep(_refreshIntervalMinutes * 60)` to convert minutes to seconds
3. For shelf app, add `refreshIntervalMinutes` configuration support from BLE (currently uses default 60 seconds)

**Example (from Sensor App)**:
```cpp
// Wait before next cycle (or sleep)
uint32_t delayMs = _refreshIntervalMinutes * 60UL * 1000UL;
delay(delayMs);  // For testing

// Optionally enter deep sleep
// if (_power) {
//     _power->enterDeepSleep(_refreshIntervalMinutes * 60); // Use refreshIntervalMinutes in seconds
// }
```

**Note**: When deep sleep is enabled, the device will restart after each sleep cycle. The main loop does not directly call deep sleep - each app manages its own sleep/wake cycle independently.

### Device Configuration

Devices can be configured in two ways:

1. **Bluetooth Low Energy (BLE)**: On cold start, the device enters BLE mode for 3 minutes, allowing wireless configuration via mobile app or web client. See [Bluetooth Configuration](#bluetooth-configuration-cold-start-ble) section for details.

2. **Remote Server**: Each device can fetch its configuration from a remote server:
   - Device-specific configs: `https://ota.denton.works/config/devices/{name}.json`
   - Fallback to: `https://ota.denton.works/config/devices/default.json`

Configuration options include:
- WiFi credentials (SSID and password)
- App mode selection (fun, sensor, shelf)
- Display refresh interval
- API keys and endpoints
- Module enable/disable toggles
- Custom message endpoints
- Per-device keys/secrets

Configuration is stored in ESP32 Preferences (NVS) and persists across deep sleep cycles and reboots.

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

## Bluetooth Configuration (Cold-Start BLE)

The device includes a Bluetooth Low Energy (BLE) configuration system that enables wireless setup without physical access. BLE is only active during cold starts (power-on reset), not when waking from deep sleep, to minimize power consumption.

### How Bluetooth Works

#### Cold-Start Detection

The `ColdStartBle` class automatically detects cold starts by checking the ESP32 wakeup cause:
- **Cold start**: `ESP_SLEEP_WAKEUP_UNDEFINED` - BLE is enabled
- **Deep sleep wake**: Any other wakeup cause - BLE is skipped

This ensures BLE only activates when the device is first powered on or manually reset, not during normal operation cycles.

#### BLE Window

When active, BLE remains enabled for up to **60 seconds** (`COLD_START_BLE_WINDOW_SECONDS`) or until a central device connects, whichever comes first. After this window expires or a connection is established, BLE is automatically disabled to conserve power.

#### WiFi/Bluetooth Coexistence

On ESP32-C3, WiFi and Bluetooth cannot operate simultaneously. When BLE is activated:
1. WiFi is explicitly disabled (`WiFi.mode(WIFI_OFF)`)
2. BLE advertising begins
3. After BLE window expires, WiFi can be re-enabled for normal operation

### skipBLE Flag Implementation

The `skipBLE` flag prevents BLE from activating on the boot immediately following a configuration update, allowing the device to immediately apply new settings without entering BLE mode again.

#### How It Works

1. **Flag Setting**: When configuration is received via BLE:
   ```cpp
   preferences.putBool("skipBLE", true);
   ```
   The flag is stored in ESP32 Preferences (NVS) before the device restarts.

2. **Flag Checking**: On the next boot, `ColdStartBle::begin()` checks the flag:
   ```cpp
   if (shouldSkipBle()) {
       // Skip BLE initialization
       return;
   }
   ```

3. **Flag Clearing**: The `shouldSkipBle()` method:
   - Reads the flag from Preferences
   - Clears it immediately after reading (one-time use)
   - Returns `true` if the flag was set, `false` otherwise

This ensures that:
- After receiving config via BLE, the device restarts and immediately applies the config (no BLE window)
- On subsequent cold starts, BLE will activate normally (flag was cleared)
- The flag is automatically managed and doesn't require manual intervention

### BLE Service Architecture

#### Device Information Service (0x180A)

Standard BLE service for device discovery:
- **Model Number Characteristic (0x2A24)**: Contains "E-Ink Display"
- Helps with service discovery and device identification

#### Custom Configuration Service

**Service UUID**: `0000ff00-0000-1000-8000-00805f9b34fb`

**TX Characteristic** (`0000ff01-0000-1000-8000-00805f9b34fb`):
- Properties: `WRITE | WRITE_NR`
- Receives JSON configuration from client
- Triggers configuration parsing and storage

**RX Characteristic** (`0000ff02-0000-1000-8000-00805f9b34fb`):
- Properties: `READ | NOTIFY`
- Can send data back to client (future use)

### Configuration Format

Configuration is sent as JSON via the TX characteristic. Example:

```json
{
  "mode": "fun",
  "wifiSSID": "YourNetwork",
  "wifiPassword": "YourPassword",
  "refreshInterval": 60,
  "timestamp": 1234567890,
  "apis": {
    "earthquake": {
      "enabled": true,
      "key": "your-api-key"
    }
  }
}
```

#### Configuration Fields

- `mode`: App mode to activate ("fun", "sensor", "shelf")
- `wifiSSID`: WiFi network name (stored in Preferences)
- `wifiPassword`: WiFi password (stored in Preferences)
- `refreshInterval`: Display refresh interval in minutes
- `timestamp`: Configuration timestamp
- `apis`: Nested object containing API-specific configuration

### Configuration Storage

All configuration is stored in ESP32 Preferences (NVS) under the "config" namespace:

- `wifiSSID`: WiFi network name
- `wifiPassword`: WiFi password
- `mode`: Active app mode
- `refreshInterval`: Refresh interval in minutes
- `timestamp`: Configuration timestamp
- `apis`: APIs configuration as JSON string
- `configJson`: Full configuration JSON string
- `skipBLE`: Boolean flag (automatically cleared after use)

### Usage Flow

1. **Cold Start**: Device powers on or is reset
   - `ColdStartBle::begin()` is called with wakeup cause
   - If cold start detected and `skipBLE` is false, BLE activates
   - Device advertises as "E-Ink Display" for 60 seconds

2. **Client Connection**: Mobile app or web client connects
   - Scans for "E-Ink Display" device
   - Connects to BLE service
   - Writes JSON configuration to TX characteristic

3. **Configuration Processing**:
   - Device receives JSON via TX characteristic callback
   - Parses and validates JSON
   - Stores configuration in Preferences
   - Sets `skipBLE` flag to `true`
   - Restarts device

4. **Configuration Application**:
   - Device reboots
   - `shouldSkipBle()` returns `true` (flag is set)
   - BLE is skipped
   - Stored configuration is loaded from Preferences
   - App manager applies configuration
   - Normal operation begins

5. **Subsequent Boots**:
   - `skipBLE` flag was cleared, so BLE activates normally on cold starts
   - Configuration persists across deep sleep cycles

### API Reference

#### ColdStartBle Class

```cpp
// Initialize BLE (call from setup())
void begin(esp_sleep_wakeup_cause_t wakeup_cause);

// Update BLE state (call from loop())
void loop();

// Check if BLE is currently active
bool isActive() const;

// Static methods for accessing stored configuration
static String getStoredWiFiSSID();
static String getStoredWiFiPassword();
static String getStoredConfigJson();
static bool hasStoredConfig();
static bool shouldSkipBle();  // Checks and clears skipBLE flag
```

### Debugging

Serial output provides detailed BLE operation information:
- BLE initialization status
- Connection events
- Configuration reception and parsing
- Storage operations
- `skipBLE` flag status

Monitor serial output at 115200 baud to see BLE activity.

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
