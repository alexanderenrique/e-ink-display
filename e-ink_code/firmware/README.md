# E-Ink Display Firmware Architecture

This document describes the modular firmware architecture for the ESP32-C3 e-ink display system. The firmware is designed around a plugin-based app system where multiple applications can run on the same hardware, each with its own functionality and configuration.

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [Core Components](#core-components)
- [App System](#app-system)
- [Available Apps](#available-apps)
- [JSON Configuration](#json-configuration)
- [Testing with JSON](#testing-with-json)
- [Adding New Apps](#adding-new-apps)
- [Build and Deployment](#build-and-deployment)

## Architecture Overview

The firmware follows a layered architecture with clear separation of concerns:

```
┌─────────────────────────────────────────┐
│           Main Application              │
│         (main.cpp)                      │
└─────────────────┬───────────────────────┘
                  │
┌─────────────────▼───────────────────────┐
│          App Manager                    │
│    (app_manager/app_manager.*)         │
│  - Manages app lifecycle                │
│  - Handles app switching                │
│  - Parses JSON configuration            │
└─────┬───────────┬───────────┬───────────┘
      │           │           │
┌─────▼─────┐ ┌──▼───┐ ┌─────▼─────┐
│  Fun App  │ │Sensor│ │ Shelf App │
│           │ │ App  │ │           │
└───────────┘ └──────┘ └───────────┘
      │           │           │
      └───────────┴───────────┘
                  │
      ┌───────────┴───────────┐
      │   Core Managers       │
      ├───────────────────────┤
      │  WiFiManager           │
      │  DisplayManager        │
      │  PowerManager          │
      │  OTAManager            │
      └───────────────────────┘
```

### Key Design Principles

1. **Dependency Injection**: Core managers are injected into apps through the `AppInterface`, allowing apps to access hardware and services without tight coupling.

2. **Plugin Architecture**: Apps implement the `AppInterface` and are registered with the `AppManager`. The active app can be switched at runtime or via JSON configuration.

3. **Separation of Concerns**: Each app is self-contained with its own:
   - `app.h/cpp` - Main app logic
   - `fetch.h/cpp` - Data fetching logic
   - `render.h/cpp` - Display rendering logic
   - `config.h` - App-specific configuration

4. **Resource Management**: Apps are responsible for managing their own resources (WiFi connections, display updates, sleep cycles) with guidance from core managers.

## Core Components

### Core Managers

Located in `firmware/core/`, these managers provide hardware abstraction and shared services:

#### WiFiManager (`core/wifi/`)
- Manages WiFi connections
- Handles connection/disconnection lifecycle
- Provides connection status checking

#### DisplayManager (`core/display/`)
- Manages e-ink display initialization and updates
- Provides rendering helpers (text wrapping, battery display)
- Handles SPI initialization and cleanup
- Supports multiple display modes (default, earthquake, ISS data)

#### PowerManager (`core/power/`)
- Battery voltage monitoring and percentage calculation
- Deep sleep management
- Wake-up reason detection
- Power optimization utilities

#### OTAManager (`core/ota/`)
- Over-the-air firmware updates
- Version checking against remote server
- Secure update handling with certificate validation

### Hardware Configuration

All hardware pin definitions and constants are centralized in `firmware/core/hardware_config.h`:

- SPI pins (SCK, MOSI, MISO, CS, DC, RST, BUSY)
- I2C pins (SDA, SCL)
- Power management pins (V_ADC, V_SWITCH)
- Battery voltage divider constants
- WiFi credentials
- OTA server configuration

## App System

### AppInterface

All apps must implement the `AppInterface` base class (`app_manager/app_interface.h`):

```cpp
class AppInterface {
public:
    virtual bool begin() = 0;              // Initialize app
    virtual void loop() = 0;               // Main app loop
    virtual void end() = 0;                // Cleanup on app switch
    virtual const char* getName() = 0;     // Return app name
    virtual bool configure(const JsonObject& config); // Optional config
    
    // Dependencies (injected by AppManager)
    void setWiFiManager(WiFiManager* wifi);
    void setDisplayManager(DisplayManager* display);
    void setPowerManager(PowerManager* power);
    void setOTAManager(OTAManager* ota);
};
```

### AppManager

The `AppManager` (`app_manager/app_manager.*`) coordinates the app system:

**Key Responsibilities:**
- Register apps with unique names
- Set active app (by name or index)
- Parse JSON configuration and apply it
- Inject core manager dependencies into apps
- Manage app lifecycle (begin/loop/end)
- Provide app query methods

**Key Methods:**
- `registerApp(AppInterface* app, const char* name)` - Register an app
- `setActiveApp(const char* name)` - Switch to an app by name
- `configureFromJson(const char* jsonString)` - Configure from JSON
- `begin()` - Start the active app
- `loop()` - Run the active app's loop

## Available Apps

### Fun App (`apps/fun/`)

Displays rotating content including:
- Room temperature and humidity (from SHT31 sensor, no WiFi needed)
- Earthquake data (requires WiFi)
- Cat facts (requires WiFi)
- ISS location data (requires WiFi)
- Useless facts (requires WiFi)

**Display Modes:** Cycles through 5 modes (0-4), persisting across deep sleep using `RTC_DATA_ATTR`.

**Dependencies:** SHT31 sensor (I2C), WiFi for API calls

### Sensor App (`apps/sensor/`)

Dedicated sensor reading and display app.

**Status:** Implementation in progress

### Shelf App (`apps/shelf/`)

Shelf/bookshelf display app.

**Status:** Implementation in progress

## JSON Configuration

The firmware supports configuration via JSON strings. This allows for:
- Runtime app selection
- App-specific configuration
- Testing different configurations without recompiling
- Future remote configuration support

### JSON Format

The JSON configuration follows this structure:

```json
{
  "app": "app_name",
  "config": {
    // App-specific configuration (optional)
  }
}
```

**Required Fields:**
- `app` (string): The name of the app to activate. Must match a registered app name.

**Optional Fields:**
- `config` (object): App-specific configuration object. Each app can define its own configuration schema.

### JSON Parsing

The `AppManager::configureFromJson()` method:
1. Parses the JSON string using ArduinoJson
2. Validates the `app` field exists
3. Finds the registered app matching the name
4. Switches to that app (calling `end()` on the previous app if switching)
5. Calls the app's `configure()` method with the `config` object (if provided)
6. Returns `true` on success, `false` on failure

**Error Handling:**
- Invalid JSON: Returns `false`, logs parse error
- Missing `app` field: Returns `false`, logs error
- Unknown app name: Returns `false`, logs error
- Configuration failure: Returns `false` if app's `configure()` returns `false`

## Testing with JSON

### Method 1: Hardcoded Test Configuration

Edit `firmware/main.cpp` and modify the `TEST_CONFIG_JSON` constant:

```cpp
// Example: Switch to sensor app
const char* TEST_CONFIG_JSON = 
    "{\"app\": \"sensor\", \"config\": {}}";

// Example: Switch to shelf app
const char* TEST_CONFIG_JSON = 
    "{\"app\": \"shelf\", \"config\": {}}";

// Example: Fun app with empty config (default)
const char* TEST_CONFIG_JSON = 
    "{\"app\": \"fun\", \"config\": {}}";
```

The configuration is applied in `setup()`:

```cpp
if (appManager.configureFromJson(TEST_CONFIG_JSON)) {
    Serial.println("[Main] Configuration loaded successfully");
} else {
    Serial.println("[Main] Configuration failed, using default");
    appManager.setActiveApp("fun"); // Fallback
}
```

### Method 2: Serial Input (Future Enhancement)

For runtime configuration, you could add Serial input parsing:

```cpp
void loop() {
    if (Serial.available()) {
        String jsonInput = Serial.readString();
        if (appManager.configureFromJson(jsonInput.c_str())) {
            Serial.println("Configuration updated!");
            appManager.begin(); // Restart with new config
        }
    }
    appManager.loop();
}
```

### JSON Examples

#### Basic App Selection

```json
{
  "app": "fun"
}
```

#### App Selection with Empty Config

```json
{
  "app": "sensor",
  "config": {}
}
```

#### App Selection with Configuration (Example)

```json
{
  "app": "fun",
  "config": {
    "displayMode": 2,
    "updateInterval": 300,
    "enableWiFi": true
  }
}
```

**Note:** The actual configuration schema depends on each app's `configure()` implementation. Currently, most apps use the default `configure()` which accepts any config and returns `true`.

### Testing Workflow

1. **Edit Configuration**: Modify `TEST_CONFIG_JSON` in `main.cpp`
2. **Build**: Compile the firmware with PlatformIO
3. **Upload**: Flash to device
4. **Monitor**: Check Serial output for configuration status
5. **Verify**: Confirm the correct app is running

### Serial Output

When configuration is applied, you'll see:

```
[AppManager] Registered app: fun
[AppManager] Registered app: sensor
[AppManager] Registered app: shelf
[AppManager] Setting active app to: fun
[AppManager] App configured successfully
[Main] Configuration loaded successfully
[FunApp] Starting Fun App
```

If configuration fails:

```
[AppManager] JSON parse error: InvalidInput
[Main] Configuration failed, using default
[AppManager] Switched to app: fun
```

## Adding New Apps

To add a new app to the system:

### 1. Create App Directory Structure

```
firmware/apps/your_app/
├── app.h
├── app.cpp
├── config.h
├── fetch.h      (optional, for data fetching)
├── fetch.cpp    (optional)
├── render.h     (optional, for display rendering)
└── render.cpp   (optional)
```

### 2. Implement AppInterface

Create your app class inheriting from `AppInterface`:

```cpp
// apps/your_app/app.h
#include "../../app_manager/app_interface.h"

class YourApp : public AppInterface {
public:
    YourApp();
    virtual ~YourApp() {}
    
    bool begin() override;
    void loop() override;
    void end() override;
    const char* getName() override { return "your_app"; }
    
    // Optional: Override configure if you need JSON config
    bool configure(const JsonObject& config) override;
    
private:
    // Your app-specific members
};
```

### 3. Implement Required Methods

```cpp
// apps/your_app/app.cpp
#include "app.h"

bool YourApp::begin() {
    Serial.println("[YourApp] Starting");
    if (_display) {
        _display->begin();
    }
    return true;
}

void YourApp::loop() {
    // Your app logic here
    // Access managers via _wifi, _display, _power, _ota
}

void YourApp::end() {
    Serial.println("[YourApp] Ending");
    // Cleanup resources
}

bool YourApp::configure(const JsonObject& config) {
    // Parse config if needed
    // Return true on success, false on failure
    return true;
}
```

### 4. Register App in main.cpp

```cpp
// Add include
#include "apps/your_app/app.h"

// Create instance
YourApp yourApp;

// Register in setup()
appManager.registerApp(&yourApp, "your_app");
```

### 5. Add JSON Configuration Support (Optional)

If your app needs configuration, implement `configure()`:

```cpp
bool YourApp::configure(const JsonObject& config) {
    if (config.containsKey("someSetting")) {
        _someSetting = config["someSetting"];
    }
    return true;
}
```

Then users can configure it:

```json
{
  "app": "your_app",
  "config": {
    "someSetting": 123
  }
}
```

## Build and Deployment

### Prerequisites

- PlatformIO installed
- ESP32-C3 board support
- Required libraries (see `platformio.ini`)

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

### OTA Updates

The firmware supports OTA updates via `OTAManager`. Configure:
- `OTA_VERSION_CHECK_URL` in `hardware_config.h`
- `OTA_PASSWORD` in `hardware_config.h`
- `ROOT_CA_CERT` in `hardware_config.h`

Apps can check for updates during their loop:

```cpp
if (_ota && _wifi && _wifi->isConnected()) {
    _ota->handle();
    if (_ota->checkForUpdate()) {
        _ota->performUpdate();
    }
}
```

## File Structure

```
firmware/
├── main.cpp                    # Entry point, app registration
├── core/                       # Core managers
│   ├── hardware_config.h      # Hardware pin definitions
│   ├── wifi/                  # WiFi management
│   ├── display/               # Display management
│   ├── power/                 # Power management
│   └── ota/                   # OTA updates
├── app_manager/               # App system
│   ├── app_interface.h        # Base app interface
│   ├── app_manager.h          # App manager header
│   └── app_manager.cpp        # App manager implementation
└── apps/                      # Application plugins
    ├── fun/                   # Fun app
    ├── sensor/                # Sensor app
    └── shelf/                 # Shelf app
```

## Troubleshooting

### App Not Found Error

- Ensure the app is registered in `main.cpp` with `registerApp()`
- Check that the app name in JSON matches the registration name (case-sensitive)
- Verify the app's `getName()` returns the expected string

### Configuration Not Applied

- Check Serial output for JSON parse errors
- Verify JSON syntax is valid (use a JSON validator)
- Ensure the app's `configure()` method returns `true`
- Check that `config` field is a JSON object, not a string

### App Not Starting

- Verify `begin()` returns `true`
- Check Serial output for initialization errors
- Ensure core managers are properly initialized before app registration
- Verify hardware connections match `hardware_config.h`

## Future Enhancements

Potential improvements to the architecture:

1. **Remote Configuration**: Load JSON config from a web server or MQTT broker
2. **App State Persistence**: Save app state to EEPROM/Flash
3. **Dynamic App Loading**: Load apps from SPIFFS or external storage
4. **Configuration Schema Validation**: Validate JSON against app-defined schemas
5. **App Dependencies**: Support for apps that depend on other apps
6. **Multi-App Mode**: Run multiple apps simultaneously (split screen, cycling)

## License

[Add your license information here]
