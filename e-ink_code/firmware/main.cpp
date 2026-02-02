#include <Arduino.h>
#include "core/wifi/wifi_manager.h"
#include "core/display/display_manager.h"
#include "core/power/power_manager.h"
#include "core/ota/ota_manager.h"
#include "app_manager/app_manager.h"
#include "apps/fun/app.h"
#include "apps/sensor/app.h"
#include "apps/shelf/app.h"
#include "esp_sleep.h"

// Core managers
WiFiManager wifiManager;
DisplayManager displayManager;
PowerManager powerManager;
OTAManager otaManager;

// App manager
AppManager appManager;

// App instances
FunApp funApp;
SensorApp sensorApp;
ShelfApp shelfApp;

// Dummy JSON configuration for testing
// Format: {"app": "app_name", "config": {...}}
// Uncomment and modify this to test different configurations
const char* TEST_CONFIG_JSON = 
    "{\"app\": \"fun\", \"config\": {}}";

// Alternative examples:
// const char* TEST_CONFIG_JSON = "{\"app\": \"sensor\", \"config\": {}}";
// const char* TEST_CONFIG_JSON = "{\"app\": \"shelf\", \"config\": {}}";

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
    
    // Initialize app manager with core managers
    appManager.setWiFiManager(&wifiManager);
    appManager.setDisplayManager(&displayManager);
    appManager.setPowerManager(&powerManager);
    appManager.setOTAManager(&otaManager);
    
    // Register all apps
    appManager.registerApp(&funApp, "fun");
    appManager.registerApp(&sensorApp, "sensor");
    appManager.registerApp(&shelfApp, "shelf");
    
    // Configure from JSON (for testing)
    // This will select the app and configure it
    if (appManager.configureFromJson(TEST_CONFIG_JSON)) {
        Serial.println("[Main] Configuration loaded successfully");
    } else {
        Serial.println("[Main] Configuration failed, using default");
        // Fallback to default app
        appManager.setActiveApp("fun");
    }
    
    // Begin the active app
    appManager.begin();
}

void loop() {
    // Let the app manager handle the active app's loop
    appManager.loop();
    
    // Note: Individual apps handle their own sleep/wake cycles
    // The app manager just coordinates which app is running
}
