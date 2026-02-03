#include <Arduino.h>
#include "core/wifi/wifi_manager.h"
#include "core/display/display_manager.h"
#include "core/power/power_manager.h"
#include "core/ota/ota_manager.h"
#include "core/bluetooth/cold_start_ble.h"
#include "app_manager/app_manager.h"
// Include only selected apps (or all if no APP_* flags, for default env)
#if defined(APP_FUN)
#include "apps/fun/app.h"
#endif
#if defined(APP_SENSOR)
#include "apps/sensor/app.h"
#endif
#if defined(APP_SHELF)
#include "apps/shelf/app.h"
#endif
#if !defined(APP_FUN) && !defined(APP_SENSOR) && !defined(APP_SHELF)
#include "apps/fun/app.h"
#include "apps/sensor/app.h"
#include "apps/shelf/app.h"
#endif
#include "esp_sleep.h"

// Core managers
WiFiManager wifiManager;
DisplayManager displayManager;
PowerManager powerManager;
OTAManager otaManager;
ColdStartBle coldStartBle;

// App manager
AppManager appManager;

// App instances (only linked when their app is enabled)
#if defined(APP_FUN)
FunApp funApp;
#endif
#if defined(APP_SENSOR)
SensorApp sensorApp;
#endif
#if defined(APP_SHELF)
ShelfApp shelfApp;
#endif
#if !defined(APP_FUN) && !defined(APP_SENSOR) && !defined(APP_SHELF)
FunApp funApp;
SensorApp sensorApp;
ShelfApp shelfApp;
#endif

// Default app name for this build (first available)
#if defined(APP_FUN) && !defined(APP_SENSOR) && !defined(APP_SHELF)
#define DEFAULT_APP_NAME "fun"
#elif defined(APP_SENSOR)
#define DEFAULT_APP_NAME "sensor"
#elif defined(APP_SHELF)
#define DEFAULT_APP_NAME "shelf"
#else
#define DEFAULT_APP_NAME "fun"  // all apps or fallback
#endif

// Dummy JSON configuration for testing
// Format: {"app": "app_name", "config": {...}}
const char* TEST_CONFIG_JSON =
    "{\"app\": \"" DEFAULT_APP_NAME "\", \"config\": {}}";

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Print wake reason
    esp_sleep_wakeup_cause_t wakeup_reason = powerManager.getWakeupCause();
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0: Serial.println("Wakeup caused by external signal using RTC_IO"); break;
        case ESP_SLEEP_WAKEUP_EXT1: Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
        case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Wakeup caused by timer"); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
        case ESP_SLEEP_WAKEUP_ULP: Serial.println("Wakeup caused by ULP program"); break;
        default: Serial.println("Wakeup was not caused by deep sleep"); break;
    }

    // On cold start only (not wake from deep sleep), enable BLE for 60s or until connected
    coldStartBle.begin(wakeup_reason);

    // Initialize app manager with core managers
    appManager.setWiFiManager(&wifiManager);
    appManager.setDisplayManager(&displayManager);
    appManager.setPowerManager(&powerManager);
    appManager.setOTAManager(&otaManager);
    
    // Register only the apps included in this build
#if defined(APP_FUN)
    appManager.registerApp(&funApp, "fun");
#endif
#if defined(APP_SENSOR)
    appManager.registerApp(&sensorApp, "sensor");
#endif
#if defined(APP_SHELF)
    appManager.registerApp(&shelfApp, "shelf");
#endif
#if !defined(APP_FUN) && !defined(APP_SENSOR) && !defined(APP_SHELF)
    appManager.registerApp(&funApp, "fun");
    appManager.registerApp(&sensorApp, "sensor");
    appManager.registerApp(&shelfApp, "shelf");
#endif

    // Configure from JSON (for testing)
    if (appManager.configureFromJson(TEST_CONFIG_JSON)) {
        Serial.println("[Main] Configuration loaded successfully");
    } else {
        Serial.println("[Main] Configuration failed, using default");
        appManager.setActiveApp(DEFAULT_APP_NAME);
    }
    
    // Begin the active app
    appManager.begin();
}

void loop() {
    // Cold-start BLE: disable after 60s or first connection
    coldStartBle.loop();

    // Let the app manager handle the active app's loop
    appManager.loop();

    // Note: Individual apps handle their own sleep/wake cycles
    // The app manager just coordinates which app is running
}
