#include "app.h"
#include "config.h"
#include "fetch.h"
#include "fun_slide.h"
#include "render.h"
#include "../../core/wifi/wifi_manager.h"
#include "../../core/display/display_manager.h"
#include "../../core/power/power_manager.h"
#include "../../core/ota/ota_manager.h"
#include "../../core/bluetooth/cold_start_ble.h"
#include <ArduinoJson.h>
#include <Wire.h>

RTC_DATA_ATTR int FunApp::displayMode = 0;

FunApp::FunApp() {
}

bool FunApp::begin() {
    Serial.println("[FunApp] Starting Fun App");

    if (_display) {
        _display->begin();
    }

    return true;
}

bool FunApp::configure(const JsonObject& config) {
    Serial.println("[FunApp] Configuring Fun App");

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

    if (config.containsKey("apis") && config["apis"].is<JsonObject>()) {
        JsonObject apis = config["apis"].as<JsonObject>();
        if (apis.containsKey("room_data")) _apiRoomData = apis["room_data"].as<bool>();
        if (apis.containsKey("cat_facts")) _apiCatFacts = apis["cat_facts"].as<bool>();
        if (apis.containsKey("earthquake")) _apiEarthquake = apis["earthquake"].as<bool>();
        if (apis.containsKey("iss")) _apiISS = apis["iss"].as<bool>();
        if (apis.containsKey("useless_facts")) _apiUselessFacts = apis["useless_facts"].as<bool>();
        if (apis.containsKey("all_new_facts")) _apiAllNewFacts = apis["all_new_facts"].as<bool>();
        if (apis.containsKey("special_messages")) _apiSpecialMessages = apis["special_messages"].as<bool>();
    }

    if (config.containsKey("displayName")) {
        String dn(config["displayName"].as<const char*>());
        dn.trim();
        if (dn.length() > 0) {
            ColdStartBle::putStoredFriendlyName(dn);
        }
    }
    if (config.containsKey("deviceId")) {
        String did(config["deviceId"].as<const char*>());
        did.trim();
        if (did.length() > 0) {
            ColdStartBle::putStoredDeviceId(did);
        }
    }

    return true;
}

void FunApp::loop() {
    int batteryPercent = -1;
    if (_power) {
        batteryPercent = _power->getBatteryPercentage();
    }

    for (int i = 0; i < 5 && !isModeEnabled(displayMode); i++) {
        displayMode = (displayMode + 1) % 5;
    }

    if (displayMode == 0) {
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

        String roomData = getRoomData();
        if (_display) {
            renderDefault(_display, roomData, batteryPercent);
        }

        if (_wifi) {
            _wifi->disconnect();
        }
    } else {
        FunSlide slide;
        bool gotSlide = false;

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
            handleOTA();

            if (_apiSpecialMessages && displayMode >= 1 && displayMode <= 4) {
                syncFunClockForSpecialHold();
                if (loadHeldSpecialSlide(slide)) {
                    gotSlide = true;
                }
            }

            bool gotViaSpecial =
                (!gotSlide && _apiSpecialMessages && displayMode >= 1 && displayMode <= 4 &&
                 (fetchSpecialSlide(slide)));  // skips normal fetch if server had a queued slide

            if (gotViaSpecial) {
                gotSlide = true;
            } else if (!gotSlide && displayMode == 1) {
                gotSlide = fetchFunScreenSlide(1, slide);
            } else if (!gotSlide && displayMode == 2) {
                if (_apiAllNewFacts) {
                    gotSlide = fetchMixedFunSlide(slide);
                } else {
                    gotSlide = fetchFunScreenSlide(2, slide);
                }
            } else if (!gotSlide && displayMode == 3) {
                gotSlide = fetchFunScreenSlide(3, slide);
            } else if (!gotSlide && displayMode == 4) {
                gotSlide = fetchFunScreenSlide(4, slide);
            }
        }

        if (gotSlide && _display) {
            renderFunSlide(_display, slide, batteryPercent);
        } else {
            Serial.println("WiFi or fetch unavailable, displaying room data...");
            String roomData = getRoomData();
            if (_display) {
                renderDefault(_display, roomData, batteryPercent);
            }
        }

        if (_wifi) {
            _wifi->disconnect();
        }
    }

    Wire.end();
    Serial.println("I2C disabled after sensor read");

    if (_display) {
        _display->disableSPI();
    }

    cycleDisplayMode();

    if (_wifi && _wifi->isConnected()) {
        handleOTA();

        if (_ota && _ota->isUpdating()) {
            Serial.println("[OTA] Update in progress, staying awake...");
            while (_ota->isUpdating()) {
                _ota->handle();
                delay(100);
            }
            Serial.println("[OTA] Update complete, restarting...");
            delay(1000);
            ESP.restart();
            return;
        }
    }

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

    if (_wifi) {
        _wifi->disconnect();
    }

    if (_display) {
        _display->hibernate();
        _display->disableSPI();
    }
}

void FunApp::handleOTA() {
    if (!_ota || !_wifi || !_wifi->isConnected()) {
        return;
    }

    _ota->setVersionCheckUrl(OTA_VERSION_CHECK_URL);
    _ota->setRootCA(ROOT_CA_CERT);
    _ota->setPassword(OTA_PASSWORD);
    _ota->setCurrentVersion(FIRMWARE_VERSION);
    _ota->begin();

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
        case 2: return _apiAllNewFacts || _apiCatFacts;
        case 3: return _apiISS;
        case 4: return !_apiAllNewFacts && _apiUselessFacts;
        default: return false;
    }
}

void FunApp::cycleDisplayMode() {
    for (int i = 0; i < 5; i++) {
        displayMode = (displayMode + 1) % 5;
        if (isModeEnabled(displayMode)) return;
    }
    displayMode = 0;
}
