#include "app.h"
#include "fetch.h"
#include "render.h"
#include "config.h"
#include "../../core/display/display_manager.h"
#include "../../core/power/power_manager.h"
#include "../../core/bluetooth/cold_start_ble.h"
#include "../../core/wifi/wifi_manager.h"

ShelfApp::ShelfApp() {
}

bool ShelfApp::configure(const JsonObject& config) {
    Serial.println("[ShelfApp] Configuring Shelf App");

    if (config.containsKey("binId")) {
        _binId = config["binId"].as<const char*>();
        Serial.print("[ShelfApp] Bin ID: ");
        Serial.println(_binId);
    } else if (config.containsKey("bin_id")) {
        _binId = config["bin_id"].as<const char*>();
        Serial.print("[ShelfApp] Bin ID: ");
        Serial.println(_binId);
    }
    
    if (config.containsKey("serverUrl")) {
        _serverUrl = config["serverUrl"].as<const char*>();
    } else if (config.containsKey("server_url")) {
        _serverUrl = config["server_url"].as<const char*>();
    }
    if (_serverUrl.length() == 0) {
        _serverUrl = SHELF_APP_DEFAULT_SERVER_URL;
    }
    Serial.print("[ShelfApp] Server URL: ");
    Serial.println(_serverUrl);

    if (config.containsKey("refreshInterval")) {
        _refreshIntervalMinutes = config["refreshInterval"].as<uint32_t>();
        if (_refreshIntervalMinutes < 1) _refreshIntervalMinutes = 1;
        Serial.print("[ShelfApp] Refresh interval: ");
        Serial.print(_refreshIntervalMinutes);
        Serial.println(" min");
    }

    return true;
}

bool ShelfApp::begin() {
    Serial.println("[ShelfApp] Starting Shelf App");
    
    // Initialize display
    if (_display) {
        _display->begin();
    }
    
    return true;
}

void ShelfApp::loop() {
    // Get battery percentage
    int batteryPercent = -1;
    if (_power) {
        batteryPercent = _power->getBatteryPercentage();
    }
    
    // Try to connect WiFi if we have bin ID and server URL configured
    bool wifiConnected = false;
    if (_wifi && _binId.length() > 0 && _serverUrl.length() > 0) {
        String wifiSSID = ColdStartBle::getStoredWiFiSSID();
        String wifiPassword = ColdStartBle::getStoredWiFiPassword();
        if (wifiSSID.length() > 0) {
            Serial.println("[ShelfApp] WiFi connection requested");
            wifiConnected = _wifi->begin(wifiSSID.c_str(), wifiPassword.c_str());
            if (wifiConnected) {
                Serial.println("[ShelfApp] WiFi connection successful - ready for server API calls");
            } else {
                Serial.println("[ShelfApp] WiFi connection failed - will show error");
            }
        } else {
            Serial.println("[ShelfApp] No WiFi credentials stored. Bin data cannot be fetched.");
        }
    }
    
    // Fetch shelf data
    String shelfData;
    if (_binId.length() > 0 && _serverUrl.length() > 0 && wifiConnected) {
        shelfData = fetchShelfData(_binId.c_str(), _serverUrl.c_str());
    } else {
        if (_binId.length() == 0) {
            shelfData = "Shelf Label\nBin ID not configured";
        } else if (_serverUrl.length() == 0) {
            shelfData = "Shelf Label\nServer URL not configured";
        } else {
            shelfData = "Shelf Label\nWiFi not connected";
        }
    }
    
    // Render shelf data
    if (_display) {
        renderShelfData(_display, shelfData, batteryPercent);
    }
    
    // Disable SPI after display update
    if (_display) {
        _display->disableSPI();
    }
    
    // Disable WiFi after fetching to save power
    if (_wifi) {
        _wifi->disconnect();
    }
    
    // Wait before next cycle (or sleep)
    uint32_t delayMs = _refreshIntervalMinutes * 60UL * 1000UL;
    Serial.print("[ShelfApp] Waiting ");
    Serial.print(_refreshIntervalMinutes);
    Serial.print(" minutes (");
    Serial.print(delayMs);
    Serial.println(" ms) before next cycle");
    delay(delayMs);
    
    // Optionally enter deep sleep
    // if (_power) {
    //     _power->enterDeepSleep(_refreshIntervalMinutes * 60); // Use refreshIntervalMinutes in seconds
    // }
}

void ShelfApp::end() {
    Serial.println("[ShelfApp] Ending Shelf App");
    
    // Disable display
    if (_display) {
        _display->hibernate();
        _display->disableSPI();
    }
}
