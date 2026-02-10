#include "app.h"
#include "render.h"
#include "config.h"
#include "../../core/display/display_manager.h"
#include "../../core/power/power_manager.h"

MessagesApp::MessagesApp() {
    for (int i = 0; i < MESSAGES_APP_MAX_MESSAGES; i++) {
        _messages[i] = "";
    }
}

bool MessagesApp::configure(const JsonObject& config) {
    Serial.println("[MessagesApp] Configuring Messages App");

    _messageCount = 0;

    if (config.containsKey("messages") && config["messages"].is<JsonArray>()) {
        JsonArray arr = config["messages"].as<JsonArray>();
        for (size_t i = 0; i < arr.size() && i < (size_t)MESSAGES_APP_MAX_MESSAGES; i++) {
            if (arr[i].is<const char*>()) {
                _messages[_messageCount++] = arr[i].as<const char*>();
            }
        }
    }

    if (_messageCount == 0) {
        for (int i = 1; i <= MESSAGES_APP_MAX_MESSAGES; i++) {
            String key = "message" + String(i);
            if (config.containsKey(key.c_str())) {
                _messages[_messageCount++] = config[key.c_str()].as<const char*>();
            }
        }
    }

    if (config.containsKey("refreshInterval")) {
        _refreshIntervalMinutes = config["refreshInterval"].as<uint32_t>();
        if (_refreshIntervalMinutes < 1) _refreshIntervalMinutes = 1;
    }
    Serial.print("[MessagesApp] Messages: ");
    Serial.println(_messageCount);
    Serial.print("[MessagesApp] Refresh: ");
    Serial.print(_refreshIntervalMinutes);
    Serial.println(" min");
    return true;
}

bool MessagesApp::begin() {
    Serial.println("[MessagesApp] Starting Messages App");

    if (_display) {
        _display->begin();
    }
    return true;
}

String MessagesApp::buildDisplayText() const {
    String out;
    for (int i = 0; i < _messageCount; i++) {
        if (_messages[i].length() > 0) {
            if (out.length() > 0) out += "\n";
            out += _messages[i];
        }
    }
    if (out.length() == 0) {
        out = "No messages configured.\nAdd messages via BLE config.";
    }
    return out;
}

void MessagesApp::loop() {
    int batteryPercent = -1;
    if (_power) {
        batteryPercent = _power->getBatteryPercentage();
    }

    String text = buildDisplayText();
    if (_display) {
        renderMessages(_display, text, batteryPercent);
    }

    if (_display) {
        _display->disableSPI();
    }

    uint32_t sleepSeconds = _refreshIntervalMinutes * 60UL;
    if (_power) {
        Serial.print("[MessagesApp] Entering deep sleep for ");
        Serial.print(_refreshIntervalMinutes);
        Serial.println(" min");
        _power->enterDeepSleep(sleepSeconds);
    } else {
        delay(sleepSeconds * 1000UL);
    }
}

void MessagesApp::end() {
    Serial.println("[MessagesApp] Ending Messages App");
    if (_display) {
        _display->hibernate();
        _display->disableSPI();
    }
}
