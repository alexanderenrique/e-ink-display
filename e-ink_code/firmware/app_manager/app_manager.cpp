#include "app_manager.h"
#include "../core/wifi/wifi_manager.h"
#include "../core/display/display_manager.h"
#include "../core/power/power_manager.h"
#include "../core/ota/ota_manager.h"
#include <ArduinoJson.h>

AppManager::AppManager() : _appCount(0), _activeAppIndex(-1), 
                           _wifi(nullptr), _display(nullptr), 
                           _power(nullptr), _ota(nullptr) {
    for (int i = 0; i < MAX_APPS; i++) {
        _apps[i] = nullptr;
        _appNames[i] = nullptr;
    }
}

void AppManager::setWiFiManager(WiFiManager* wifi) {
    _wifi = wifi;
}

void AppManager::setDisplayManager(DisplayManager* display) {
    _display = display;
}

void AppManager::setPowerManager(PowerManager* power) {
    _power = power;
}

void AppManager::setOTAManager(OTAManager* ota) {
    _ota = ota;
}

void AppManager::registerApp(AppInterface* app, const char* name) {
    if (_appCount >= MAX_APPS) {
        Serial.println("[AppManager] Maximum number of apps reached!");
        return;
    }
    
    if (app == nullptr || name == nullptr) {
        Serial.println("[AppManager] Invalid app or name!");
        return;
    }
    
    // Inject dependencies
    app->setWiFiManager(_wifi);
    app->setDisplayManager(_display);
    app->setPowerManager(_power);
    app->setOTAManager(_ota);
    
    _apps[_appCount] = app;
    _appNames[_appCount] = name;
    _appCount++;
    
    Serial.print("[AppManager] Registered app: ");
    Serial.println(name);
    
    // Set first app as active if none is set
    if (_activeAppIndex == -1) {
        _activeAppIndex = 0;
    }
}

void AppManager::setActiveApp(const char* name) {
    for (int i = 0; i < _appCount; i++) {
        if (strcmp(_appNames[i], name) == 0) {
            if (_activeAppIndex >= 0 && _activeAppIndex < _appCount) {
                _apps[_activeAppIndex]->end();
            }
            _activeAppIndex = i;
            _apps[_activeAppIndex]->begin();
            Serial.print("[AppManager] Switched to app: ");
            Serial.println(name);
            return;
        }
    }
    Serial.print("[AppManager] App not found: ");
    Serial.println(name);
}

void AppManager::setActiveApp(int index) {
    if (index < 0 || index >= _appCount) {
        Serial.print("[AppManager] Invalid app index: ");
        Serial.println(index);
        return;
    }
    
    if (_activeAppIndex >= 0 && _activeAppIndex < _appCount) {
        _apps[_activeAppIndex]->end();
    }
    
    _activeAppIndex = index;
    _apps[_activeAppIndex]->begin();
    
    Serial.print("[AppManager] Switched to app index: ");
    Serial.print(index);
    Serial.print(" (");
    Serial.print(_appNames[index]);
    Serial.println(")");
}

bool AppManager::configureFromJson(const char* jsonString) {
    if (jsonString == nullptr) {
        Serial.println("[AppManager] Invalid JSON string");
        return false;
    }
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        Serial.print("[AppManager] JSON parse error: ");
        Serial.println(error.c_str());
        return false;
    }
    
    // Get app name
    if (!doc.containsKey("app")) {
        Serial.println("[AppManager] JSON missing 'app' field");
        return false;
    }
    
    const char* appName = doc["app"];
    if (appName == nullptr) {
        Serial.println("[AppManager] Invalid app name");
        return false;
    }
    
    // Find and set active app
    bool appFound = false;
    for (int i = 0; i < _appCount; i++) {
        if (strcmp(_appNames[i], appName) == 0) {
            // End current app if switching
            if (_activeAppIndex >= 0 && _activeAppIndex < _appCount && _activeAppIndex != i) {
                _apps[_activeAppIndex]->end();
            }
            
            _activeAppIndex = i;
            appFound = true;
            Serial.print("[AppManager] Setting active app to: ");
            Serial.println(appName);
            break;
        }
    }
    
    if (!appFound) {
        Serial.print("[AppManager] App not found: ");
        Serial.println(appName);
        return false;
    }
    
    // Configure the app if config is provided
    if (doc.containsKey("config") && doc["config"].is<JsonObject>()) {
        JsonObject config = doc["config"].as<JsonObject>();
        if (_apps[_activeAppIndex]->configure(config)) {
            Serial.println("[AppManager] App configured successfully");
        } else {
            Serial.println("[AppManager] App configuration failed");
            return false;
        }
    }
    
    return true;
}

void AppManager::begin() {
    if (_activeAppIndex >= 0 && _activeAppIndex < _appCount) {
        _apps[_activeAppIndex]->begin();
    }
}

void AppManager::loop() {
    if (_activeAppIndex >= 0 && _activeAppIndex < _appCount) {
        _apps[_activeAppIndex]->loop();
    }
}

int AppManager::getAppCount() {
    return _appCount;
}

const char* AppManager::getAppName(int index) {
    if (index < 0 || index >= _appCount) {
        return nullptr;
    }
    return _appNames[index];
}

int AppManager::getActiveAppIndex() {
    return _activeAppIndex;
}

const char* AppManager::getActiveAppName() {
    if (_activeAppIndex >= 0 && _activeAppIndex < _appCount) {
        return _appNames[_activeAppIndex];
    }
    return nullptr;
}

bool AppManager::hasApp(const char* name) {
    if (name == nullptr) {
        return false;
    }
    for (int i = 0; i < _appCount; i++) {
        if (strcmp(_appNames[i], name) == 0) {
            return true;
        }
    }
    return false;
}
