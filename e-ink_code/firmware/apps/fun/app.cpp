#include "app.h"
#include "config.h"
#include "fetch.h"
#include "render.h"
#include "../../core/wifi/wifi_manager.h"
#include "../../core/display/display_manager.h"
#include "../../core/power/power_manager.h"
#include "../../core/ota/ota_manager.h"
#include "../../core/bluetooth/cold_start_ble.h"
#include <ArduinoJson.h>
#include <Wire.h>

// Static display mode (persists across deep sleep)
RTC_DATA_ATTR int FunApp::displayMode = 0;

FunApp::FunApp() {
}

bool FunApp::begin() {
    Serial.println("[FunApp] Starting Fun App");
    
    // Initialize display
    if (_display) {
        _display->begin();
    }
    
    return true;
}

bool FunApp::configure(const JsonObject& config) {
    Serial.println("[FunApp] Configuring Fun App");
    
    // Get refresh interval from config (in minutes)
    if (config.containsKey("refreshInterval")) {
        _refreshIntervalMinutes = config["refreshInterval"];
        Serial.print("[FunApp] Refresh interval set to: ");
        Serial.print(_refreshIntervalMinutes);
        Serial.println(" minutes");
    } else {
        Serial.print("[FunApp] No refreshInterval in config, using default: ");
        Serial.print(_refreshIntervalMinutes);
        Serial.println(" minutes");
    }
    
    // API toggles: only enabled APIs are shown when cycling
    if (config.containsKey("apis") && config["apis"].is<JsonObject>()) {
        JsonObject apis = config["apis"].as<JsonObject>();
        if (apis.containsKey("room_data")) _apiRoomData = apis["room_data"].as<bool>();
        if (apis.containsKey("cat_facts")) _apiCatFacts = apis["cat_facts"].as<bool>();
        if (apis.containsKey("earthquake")) _apiEarthquake = apis["earthquake"].as<bool>();
        if (apis.containsKey("iss")) _apiISS = apis["iss"].as<bool>();
        if (apis.containsKey("useless_facts")) _apiUselessFacts = apis["useless_facts"].as<bool>();
    }
    
    return true;
}

void FunApp::loop() {
    // Get battery percentage
    int batteryPercent = -1;
    if (_power) {
        batteryPercent = _power->getBatteryPercentage();
    }
    
    // Skip disabled modes so we only show content for APIs enabled in config
    for (int i = 0; i < 5 && !isModeEnabled(displayMode); i++) {
        displayMode = (displayMode + 1) % 5;
    }
    
    // Mode 0: Room temperature and humidity (WiFi optional for strength display)
    // Mode 1: Earthquake, 2: Meow fact, 3: ISS, 4: Useless fact (WiFi needed)
    if (displayMode == 0) {
        // Try to connect WiFi to show WiFi strength if available
        if (_wifi) {
            String wifiSSID = ColdStartBle::getStoredWiFiSSID();
            String wifiPassword = ColdStartBle::getStoredWiFiPassword();
            
            if (wifiSSID.length() > 0) {
                Serial.print("[FunApp] Connecting to WiFi for room data display: ");
                Serial.println(wifiSSID);
                _wifi->begin(wifiSSID.c_str(), wifiPassword.c_str());
            } else {
                Serial.println("[FunApp] No WiFi credentials stored. Room data will display without WiFi strength.");
            }
        }
        
        // Display temperature and humidity from SHT31 sensor
        // WiFi strength will be included if WiFi is connected, otherwise omitted
        String roomData = getRoomData();
        if (_display) {
            renderDefault(_display, roomData, batteryPercent);
        }
        
        // Disable WiFi after displaying to save power
        if (_wifi) {
            _wifi->disconnect();
        }
    } else {
        // For API calls, initialize WiFi once at the start
        // Get WiFi credentials from stored config (set via BLE)
        if (_wifi) {
            String wifiSSID = ColdStartBle::getStoredWiFiSSID();
            String wifiPassword = ColdStartBle::getStoredWiFiPassword();
            
            if (wifiSSID.length() > 0) {
                Serial.print("[FunApp] Connecting to WiFi: ");
                Serial.println(wifiSSID);
                _wifi->begin(wifiSSID.c_str(), wifiPassword.c_str());
            } else {
                Serial.println("[FunApp] WARNING: No WiFi credentials stored. WiFi features disabled.");
            }
        }
        
        if (_wifi && _wifi->isConnected()) {
            // Initialize OTA when WiFi is connected
            handleOTA();
            
            if (displayMode == 1) {
                Serial.println("Fetching earthquake fact...");
                String fact = getEarthQuakeFact();
                Serial.println("Earthquake Fact: " + fact);
                if (_display) {
                    renderEarthquakeFact(_display, fact, batteryPercent);
                }
            } else if (displayMode == 2) {
                Serial.println("Fetching meow fact...");
                String fact = fetchMeowFact();
                Serial.println("Meow Fact: " + fact);
                if (_display) {
                    renderDefault(_display, fact, batteryPercent);
                }
            } else if (displayMode == 3) {
                Serial.println("Fetching ISS data...");
                String issData = getISSData();
                Serial.println("ISS Data: " + issData);
                if (_display) {
                    renderISSData(_display, issData, batteryPercent);
                }
            } else if (displayMode == 4) {
                Serial.println("Fetching useless fact...");
                String fact = fetchUselessFact();
                Serial.println("Useless Fact: " + fact);
                if (_display) {
                    renderDefault(_display, fact, batteryPercent);
                }
            }
        } else {
            // WiFi failed, fall back to room data
            Serial.println("WiFi not available, displaying room data...");
            String roomData = getRoomData();
            if (_display) {
                renderDefault(_display, roomData, batteryPercent);
            }
        }
        
        // Disable WiFi after all API calls are done
        if (_wifi) {
            _wifi->disconnect();
        }
    }
    
    // Disable I2C after reading sensor
    Wire.end();
    Serial.println("I2C disabled after sensor read");
    
    // Disable SPI after display update
    if (_display) {
        _display->disableSPI();
    }
    
    // Cycle to next display mode (0-4, then back to 0)
    cycleDisplayMode();
    
    // Handle OTA updates before entering sleep
    if (_wifi && _wifi->isConnected()) {
        handleOTA();
        
        // If OTA update is in progress, stay awake and handle it
        if (_ota && _ota->isUpdating()) {
            Serial.println("[OTA] Update in progress, staying awake...");
            while (_ota->isUpdating()) {
                _ota->handle();
                delay(100);
            }
            Serial.println("[OTA] Update complete, restarting...");
            delay(1000);
            ESP.restart();
            return; // Never reached, but good practice
        }
    }
    
    // Enter deep sleep until next cycle (or delay if no power manager)
    uint32_t sleepSeconds = _refreshIntervalMinutes * 60UL;
    if (_power) {
        Serial.print("[FunApp] Entering deep sleep for ");
        Serial.print(_refreshIntervalMinutes);
        Serial.println(" min");
        _power->enterDeepSleep(sleepSeconds);
    } else {
        delay(sleepSeconds * 1000UL);
    }
}

void FunApp::end() {
    Serial.println("[FunApp] Ending Fun App");
    
    // Disable WiFi
    if (_wifi) {
        _wifi->disconnect();
    }
    
    // Disable display
    if (_display) {
        _display->hibernate();
        _display->disableSPI();
    }
}

void FunApp::handleOTA() {
    if (!_ota || !_wifi || !_wifi->isConnected()) {
        return;
    }
    
    // Configure OTA
    _ota->setVersionCheckUrl(OTA_VERSION_CHECK_URL);
    _ota->setRootCA(ROOT_CA_CERT);
    _ota->setPassword(OTA_PASSWORD);
    _ota->setCurrentVersion(FIRMWARE_VERSION);
    _ota->begin();
    
    // Check for updates
    _ota->handle();
    if (_ota->checkForUpdate()) {
        Serial.println("[FunApp] Update available, performing update...");
        _ota->performUpdate();
    }
}

bool FunApp::isModeEnabled(int mode) const {
    switch (mode) {
        case 0: return _apiRoomData;
        case 1: return _apiEarthquake;
        case 2: return _apiCatFacts;
        case 3: return _apiISS;
        case 4: return _apiUselessFacts;
        default: return false;
    }
}

void FunApp::cycleDisplayMode() {
    // Advance to next mode, then skip any disabled until we find an enabled one (or wrap)
    for (int i = 0; i < 5; i++) {
        displayMode = (displayMode + 1) % 5;
        if (isModeEnabled(displayMode)) return;
    }
    // If all disabled, leave at 0 (room data as fallback)
    displayMode = 0;
}
