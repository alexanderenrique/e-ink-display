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
    // Find the first non-empty message
    _currentMessageIndex = 0;
    for (int i = 0; i < _messageCount; i++) {
        if (_messages[i].length() > 0) {
            _currentMessageIndex = i;
            break;
        }
    }

    if (_display) {
        _display->begin();
    }
    return true;
}

String MessagesApp::buildDisplayText() const {
    // Return the current message if it's not empty
    if (_currentMessageIndex < _messageCount && _messages[_currentMessageIndex].length() > 0) {
        return _messages[_currentMessageIndex];
    }
    
    // If no valid message found, return default message
    return "No messages configured.\nAdd messages via BLE config.";
}

void MessagesApp::advanceToNextMessage() {
    // Find the next non-empty message
    int startIndex = _currentMessageIndex;
    int attempts = 0;
    
    do {
        _currentMessageIndex = (_currentMessageIndex + 1) % _messageCount;
        attempts++;
        
        // If we've checked all messages and none are valid, stop
        if (attempts >= _messageCount) {
            // Reset to start if we've gone through all messages
            _currentMessageIndex = startIndex;
            break;
        }
    } while (_messages[_currentMessageIndex].length() == 0);
}

void MessagesApp::loop() {
    int batteryPercent = -1;
    if (_power) {
        batteryPercent = _power->getBatteryPercentage();
    }

    // Display the current message
    String text = buildDisplayText();
    if (_display) {
        renderMessages(_display, text, batteryPercent);
    }

    if (_display) {
        _display->disableSPI();
    }

    // Advance to the next message for next loop iteration
    advanceToNextMessage();

    // Sleep for the refresh interval before displaying next message
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
