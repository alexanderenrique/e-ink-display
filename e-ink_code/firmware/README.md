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
- Optional one-off slides pushed from the aggregator (**special messages**; requires WiFi, device UUID, and server-side queue — see **`apis.special_messages`** below)

**Display Modes:** Cycles through 5 modes (0-4), persisting across deep sleep using `RTC_DATA_ATTR`.

**WiFi / aggregator usage:** The ESP only calls the fun HTTP endpoints for modes that are enabled in configuration (see **`config.apis`** below). The server does not enforce per-mode policy; it responds to whatever the client requests once `X-Fun-Key` matches (if the server is configured with `FUN_API_KEY`).

### Fun app `apis` flags (BLE / stored JSON)

When the fun app is configured (including data loaded from NVS after provisioning over BLE), optional `config.apis` booleans turn each **display mode** on or off:

| Key | Mode | Content |
|-----|------|--------|
| `room_data` | 0 | Room temp/humidity (and WiFi RSSI when connected) |
| `earthquake` | 1 | Earthquake slide (WiFi) |
| `cat_facts` | 2 | Cat facts — one `GET /v1/fun/screen?m=2` per wake (WiFi), unless `all_new_facts` |
| `iss` | 3 | ISS slide (WiFi) |
| `useless_facts` | 4 | Useless facts — one `GET /v1/fun/screen?m=4` per wake (WiFi); ignored when `all_new_facts` is on |
| `all_new_facts` | (uses mode 2) | When `true`, mode 2 calls `GET /v1/fun/facts/mixed?count=1` so the device shows a random slide from **any** non-empty `data/*_facts.json` pool on the server; mode 4 is skipped so cat vs useless toggles do not duplicate a second facts slot |
| `special_messages` | (WiFi modes 1–4) | When `true` (default), each wake in an online WiFi mode first calls `GET /v1/fun/special` using the stored device UUID (`X-Device-Id`). If the server returns a slide, that slide is shown **instead of** the normal earthquake/cat/ISS/mixed/useless slide for this cycle (the message is dequeued server-side). If the server responds with no body (`204`), the firmware continues with the usual mode fetch |

Fact text is always fetched live over WiFi for those modes (no NVS queue). The server still exposes `GET /v1/fun/facts/batch` for other clients; the fun app firmware no longer uses it.

**Defaults:** If there is no `apis` object (e.g. `{"app":"fun","config":{}}` in [`main.cpp`](main.cpp) when nothing is stored from BLE), **all flags default to on** in [`apps/fun/app.h`](apps/fun/app.h). To limit network use, set `apis` explicitly, for example:

```json
{
  "app": "fun",
  "config": {
    "refreshInterval": 2,
    "apis": {
      "room_data": true,
      "cat_facts": true,
      "iss": true,
      "earthquake": false,
      "useless_facts": false,
      "all_new_facts": false,
      "special_messages": true
    }
  }
}
```

BLE provisioning stores a top-level `apis` object that [`main.cpp`](main.cpp) merges into `config` when building the app-manager JSON.

**Device identity (aggregator / special messages):** In top-level provisioning JSON you can send a human-readable label (any of `displayName`, `deviceFriendlyName`, or `friendly_name`) which is persisted as `deviceFriendlyName` in Preferences and sent on each aggregator request as the `X-Device-Name` header. Optionally include `deviceId` / `device_id` (UUID) if you mint the ID in the provisioning app (**phone-mint**); otherwise NVS keeps `deviceId` empty until the firmware calls `POST /v1/devices/register` once (server-mint UUID), then repeats that id as `X-Device-Id` on later calls. To queue a one-off slide for a given UUID (or a named group), use the server admin API documented in [server README — Special messages](../server/README.md#special-messages-targeted-slides).

**Special messages — did the server / device accept it?**

- **Enqueue accepted (admin):** A successful `POST /v1/admin/special` returns HTTP **200** with JSON like `{"ok":true,"enqueued_for":["…uuid…"],"unknown_groups":[]}` — **`enqueued_for`** is the definitive list of device UUIDs that received a queued copy. If **`ok`** is **`false`**, read **`error`**; **`unknown_groups`** means one or more **`group_ids`** were not defined in the on-disk store yet (define them with **`groups`** in the same request, or persist them beforehand — see below).
- **Still waiting vs. consumed (device):** There is **no separate HTTP endpoint** for delivery receipts. Pending items live in the server file **`FUN_SPECIAL_STORE`** (default `data/special_messages.json`; often **`/var/lib/fun-aggregator/special_messages.json`** on a Pi — see server README). It is structured as **`queues`** (per-device UUID FIFO arrays) and **`groups`** (named lists of UUIDs). After enqueue, **`queues.<device-uuid>`** holds pending slide objects until the firmware’s next successful **`GET /v1/fun/special`**; that POP **removes** the head entry and returns the slide, so seeing the slide on the ink display is the strongest proof the device consumed it. Inspect the JSON on the Pi (or `jq '.queues."YOUR-DEVICE-UUID"' /path/to/special_messages.json`) to see what is **still queued**; **`204`** on **`/v1/fun/special`** means nothing left for that UUID. Firmware serial output logs HTTP/JSON failures for **`/v1/fun/special`** (see `[FunFetch] Special slide …`); a clean fetch does not spam success logs — use the screen or the store file.

**Special messages — grouping UUIDs**

Named groups are **server-side strings** pointing at **lists of device UUID strings** in **`FUN_SPECIAL_STORE.groups`**. Typical workflow from your laptop against the public API (same pattern on the Pi with `http://127.0.0.1:8081`):

1. **Define or update a group** and **enqueue in one request** — the **`groups`** object is merged **before** **`group_ids`** is resolved for that POST:

```bash
curl -sS -X POST 'https://YOUR-HOST/v1/admin/special' \
  -H 'Content-Type: application/json' \
  -H 'X-Fun-Admin-Key: YOUR_ADMIN_SECRET' \
  -d '{"text":"Hi everyone","group_ids":["book_club"],"groups":{"book_club":["uuid-1-here","uuid-2-here"]}}'
```

2. **Later posts** can reference **`group_ids":["book_club"]`** alone if that group already exists in the JSON file — or supply **`groups`** again to replace the membership (the server **replaces** the list for that name when **`groups`** is present; an **empty array** **`"groups":{"book_club":[]}`** **removes** that group mapping).

You can combine **`device_ids`** and **`group_ids`** in the same body; duplicates are merged. Full curl examples and env vars (**`FUN_ADMIN_API_KEY`**, **`FUN_SPECIAL_STORE`**) are in [server README — Special messages](../server/README.md#special-messages-targeted-slides).

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

Firmware in this directory is licensed under the MIT License. See
[`LICENSE-SOFTWARE`](../../LICENSE-SOFTWARE) and the overview in
[`LICENSE`](../../LICENSE).
