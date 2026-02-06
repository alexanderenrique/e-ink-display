#include <Arduino.h>
#include "core/wifi/wifi_manager.h"
#include "core/display/display_manager.h"
#include "core/power/power_manager.h"
#include "core/ota/ota_manager.h"
#include "core/bluetooth/cold_start_ble.h"
#include "app_manager/app_manager.h"
#include <ArduinoJson.h>
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
#include "hardware_config.h"

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

    // Check battery level on wakeup
    // If we woke from timer (likely from low battery sleep), check if battery has recovered
    int batteryPercent = powerManager.getBatteryPercentage();
    Serial.print("[Main] Battery level: ");
    Serial.print(batteryPercent);
    Serial.println("%");
    
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        // We woke from timer - could be from low battery sleep or normal sleep
        // Check if battery is still low
        if (batteryPercent < BATTERY_RESUME_THRESHOLD_PERCENT) {
            Serial.println("[Main] Battery still low, showing message and entering low battery sleep");
            displayManager.begin();
            displayManager.displayLowBatteryMessage();
            delay(2000); // Give time for message to be visible
            powerManager.enterLowBatterySleep();
            // Code never reaches here
            return;
        } else {
            Serial.println("[Main] Battery recovered, resuming normal operation");
        }
    }
    
    // Check if battery is critically low (before starting normal operation)
    if (batteryPercent <= BATTERY_LOW_THRESHOLD_PERCENT) {
        Serial.println("[Main] Battery critically low, showing message and entering low battery sleep");
        displayManager.begin();
        displayManager.displayLowBatteryMessage();
        delay(2000); // Give time for message to be visible
        powerManager.enterLowBatterySleep();
        // Code never reaches here
        return;
    }

    // On cold start only (not wake from deep sleep), enable BLE for 60s or until connected
    coldStartBle.begin(wakeup_reason);

    // When in BLE config mode, show config screen on display
    if (coldStartBle.isActive()) {
        displayManager.begin();
        displayManager.displayBluetoothConfigMode();
    }

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

    // Try to load configuration from Preferences (stored via BLE)
    String storedConfigJson = ColdStartBle::getStoredConfigJson();
    if (storedConfigJson.length() > 0) {
        Serial.println("[Main] Found stored configuration from BLE");
        Serial.print("[Main] Config JSON: ");
        Serial.println(storedConfigJson);
        
        // Parse and apply the stored config
        // The stored config format is: {"mode":"fun","timestamp":...,"refreshInterval":60,"apis":{...},"wifiSSID":"...","wifiPassword":"..."}
        // We need to convert it to app manager format: {"app":"fun","config":{...}}
        // For now, let's use ArduinoJson to transform it
        DynamicJsonDocument storedDoc(2048);
        DeserializationError error = deserializeJson(storedDoc, storedConfigJson);
        
        if (!error && storedDoc.containsKey("mode")) {
            // Create app manager format
            DynamicJsonDocument appDoc(2048);
            appDoc["app"] = storedDoc["mode"];
            
            // Copy config fields (refreshInterval, apis, sensor app fields, etc.) to config object
            JsonObject config = appDoc.createNestedObject("config");
            if (storedDoc.containsKey("refreshInterval")) {
                config["refreshInterval"] = storedDoc["refreshInterval"];
            }
            if (storedDoc.containsKey("apis")) {
                config["apis"] = storedDoc["apis"];
            }
            // Sensor app: temperature units (C/F), nemo token/url/sensor ID, location
            if (storedDoc.containsKey("units")) {
                config["units"] = storedDoc["units"];
            } else if (storedDoc.containsKey("temperatureUnit")) {
                config["units"] = storedDoc["temperatureUnit"];
            }
            if (storedDoc.containsKey("nemoToken")) {
                config["nemoToken"] = storedDoc["nemoToken"];
            } else if (storedDoc.containsKey("nemo_token")) {
                config["nemoToken"] = storedDoc["nemo_token"];
            }
            if (storedDoc.containsKey("nemoUrl")) {
                config["nemoUrl"] = storedDoc["nemoUrl"];
            } else if (storedDoc.containsKey("nemo_url")) {
                config["nemoUrl"] = storedDoc["nemo_url"];
            } else if (storedDoc.containsKey("nemoApiEndpoint")) {
                config["nemoUrl"] = storedDoc["nemoApiEndpoint"];
            }
            // Handle temperature and humidity sensor IDs separately
            if (storedDoc.containsKey("temperatureSensorId")) {
                config["temperatureSensorId"] = storedDoc["temperatureSensorId"];
            } else if (storedDoc.containsKey("temperature_sensor_id")) {
                config["temperatureSensorId"] = storedDoc["temperature_sensor_id"];
            }
            if (storedDoc.containsKey("humiditySensorId")) {
                config["humiditySensorId"] = storedDoc["humiditySensorId"];
            } else if (storedDoc.containsKey("humidity_sensor_id")) {
                config["humiditySensorId"] = storedDoc["humidity_sensor_id"];
            }
            // Legacy support for single sensorId (for backwards compatibility)
            if (storedDoc.containsKey("sensorId") && !config.containsKey("temperatureSensorId")) {
                config["temperatureSensorId"] = storedDoc["sensorId"];
            } else if (storedDoc.containsKey("sensor_id") && !config.containsKey("temperatureSensorId")) {
                config["temperatureSensorId"] = storedDoc["sensor_id"];
            } else if (storedDoc.containsKey("nemoSensorId") && !config.containsKey("temperatureSensorId")) {
                config["temperatureSensorId"] = storedDoc["nemoSensorId"];
            }
            if (storedDoc.containsKey("sensorLocation")) {
                config["sensorLocation"] = storedDoc["sensorLocation"];
            } else if (storedDoc.containsKey("sensor_location")) {
                config["sensorLocation"] = storedDoc["sensor_location"];
            }
            // Shelf app: bin ID, server host, server port
            if (storedDoc.containsKey("binId")) {
                config["binId"] = storedDoc["binId"];
            } else if (storedDoc.containsKey("bin_id")) {
                config["binId"] = storedDoc["bin_id"];
            }
            if (storedDoc.containsKey("serverHost")) {
                config["serverHost"] = storedDoc["serverHost"];
            } else if (storedDoc.containsKey("server_host")) {
                config["serverHost"] = storedDoc["server_host"];
            }
            if (storedDoc.containsKey("serverPort")) {
                config["serverPort"] = storedDoc["serverPort"];
            } else if (storedDoc.containsKey("server_port")) {
                config["serverPort"] = storedDoc["server_port"];
            }
            // Legacy support: if serverUrl is provided, try to parse it
            if (storedDoc.containsKey("serverUrl") || storedDoc.containsKey("server_url")) {
                String serverUrl = storedDoc.containsKey("serverUrl") 
                    ? storedDoc["serverUrl"].as<const char*>()
                    : storedDoc["server_url"].as<const char*>();
                // Try to parse URL format: http://host:port or host:port
                int protocolEnd = serverUrl.indexOf("://");
                String hostPort = (protocolEnd >= 0) ? serverUrl.substring(protocolEnd + 3) : serverUrl;
                int colonPos = hostPort.indexOf(':');
                if (colonPos > 0) {
                    config["serverHost"] = hostPort.substring(0, colonPos);
                    config["serverPort"] = hostPort.substring(colonPos + 1).toInt();
                } else {
                    config["serverHost"] = hostPort;
                }
            }
            
            String appConfigJson;
            serializeJson(appDoc, appConfigJson);
            
            Serial.print("[Main] Transformed config: ");
            Serial.println(appConfigJson);
            
            if (appManager.configureFromJson(appConfigJson.c_str())) {
                Serial.println("[Main] Stored configuration loaded successfully");
            } else {
                Serial.println("[Main] Failed to apply stored config, using default");
                appManager.setActiveApp(DEFAULT_APP_NAME);
            }
        } else {
            Serial.println("[Main] Failed to parse stored config, using default");
            appManager.setActiveApp(DEFAULT_APP_NAME);
        }
    } else {
        Serial.println("[Main] No stored configuration found, using test config");
        // Configure from JSON (for testing)
        if (appManager.configureFromJson(TEST_CONFIG_JSON)) {
            Serial.println("[Main] Test configuration loaded successfully");
        } else {
            Serial.println("[Main] Configuration failed, using default");
            appManager.setActiveApp(DEFAULT_APP_NAME);
        }
    }
    
    // Begin the active app
    appManager.begin();
}

void loop() {
    // Cold-start BLE: disable after 2 minutes or first connection
    coldStartBle.loop();

    // Check battery level before running app
    int batteryPercent = powerManager.getBatteryPercentage();
    if (batteryPercent <= BATTERY_LOW_THRESHOLD_PERCENT) {
        Serial.println("[Main] Battery critically low during operation, showing message and entering low battery sleep");
        displayManager.begin();
        displayManager.displayLowBatteryMessage();
        delay(2000); // Give time for message to be visible
        powerManager.enterLowBatterySleep();
        // Code never reaches here
        return;
    }

    // While BLE config mode is active, keep showing the config screen (don't run app loop).
    // After BLE times out or config is received, run the app and it will update the display.
    if (!coldStartBle.isActive()) {
        appManager.loop();
    }

    // Note: Individual apps handle their own sleep/wake cycles
    // The app manager just coordinates which app is running
}
