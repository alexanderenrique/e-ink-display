#include "app.h"
#include "fetch.h"
#include "render.h"
#include "../../core/display/display_manager.h"
#include "../../core/power/power_manager.h"
#include "../../core/bluetooth/cold_start_ble.h"
#include "../../core/wifi/wifi_manager.h"

SensorApp::SensorApp() {
}

bool SensorApp::configure(const JsonObject& config) {
    Serial.println("[SensorApp] Configuring Sensor App");

    if (config.containsKey("units")) {
        _units = config["units"].as<const char*>();
        if (_units != "C") _units = "F";
        Serial.print("[SensorApp] Units: ");
        Serial.println(_units);
    }
    if (config.containsKey("refreshInterval")) {
        _refreshIntervalMinutes = config["refreshInterval"].as<uint32_t>();
        if (_refreshIntervalMinutes < 1) _refreshIntervalMinutes = 1;
        Serial.print("[SensorApp] Refresh interval: ");
        Serial.print(_refreshIntervalMinutes);
        Serial.println(" min");
    }
    if (config.containsKey("nemoToken")) {
        _nemoToken = config["nemoToken"].as<const char*>();
    } else if (config.containsKey("nemo_token")) {
        _nemoToken = config["nemo_token"].as<const char*>();
    }
    if (_nemoToken.length() > 0) Serial.println("[SensorApp] Nemo token set");

    if (config.containsKey("nemoUrl")) {
        _nemoUrl = config["nemoUrl"].as<const char*>();
    } else if (config.containsKey("nemo_url")) {
        _nemoUrl = config["nemo_url"].as<const char*>();
    }
    if (_nemoUrl.length() > 0) {
        Serial.print("[SensorApp] Nemo URL: ");
        Serial.println(_nemoUrl);
    }

    if (config.containsKey("temperatureSensorId")) {
        _temperatureSensorId = config["temperatureSensorId"].as<const char*>();
    } else if (config.containsKey("temperature_sensor_id")) {
        _temperatureSensorId = config["temperature_sensor_id"].as<const char*>();
    }
    if (_temperatureSensorId.length() > 0) {
        Serial.print("[SensorApp] Temperature Sensor ID: ");
        Serial.println(_temperatureSensorId);
    }

    if (config.containsKey("humiditySensorId")) {
        _humiditySensorId = config["humiditySensorId"].as<const char*>();
    } else if (config.containsKey("humidity_sensor_id")) {
        _humiditySensorId = config["humidity_sensor_id"].as<const char*>();
    }
    if (_humiditySensorId.length() > 0) {
        Serial.print("[SensorApp] Humidity Sensor ID: ");
        Serial.println(_humiditySensorId);
    }

    if (config.containsKey("sensorLocation")) {
        _sensorLocation = config["sensorLocation"].as<const char*>();
    } else if (config.containsKey("sensor_location")) {
        _sensorLocation = config["sensor_location"].as<const char*>();
    }
    if (_sensorLocation.length() > 0) {
        Serial.print("[SensorApp] Sensor location: ");
        Serial.println(_sensorLocation);
    }

    return true;
}

bool SensorApp::begin() {
    Serial.println("[SensorApp] Starting Sensor App");

    if (!initSensor()) {
        Serial.println("[SensorApp] SHT31 init failed; display will show error when fetching.");
    }

    if (_display) {
        _display->begin();
    }

    return true;
}

void SensorApp::loop() {
    int batteryPercent = -1;
    if (_power) {
        batteryPercent = _power->getBatteryPercentage();
    }

    // Try to connect WiFi to show WiFi strength if available
    // Also connect if we have Nemo config for posting data
    bool wifiConnected = false;
    if (_wifi) {
        String wifiSSID = ColdStartBle::getStoredWiFiSSID();
        String wifiPassword = ColdStartBle::getStoredWiFiPassword();
        if (wifiSSID.length() > 0) {
            Serial.println("[SensorApp] WiFi connection requested");
            wifiConnected = _wifi->begin(wifiSSID.c_str(), wifiPassword.c_str());
            if (wifiConnected) {
                Serial.println("[SensorApp] WiFi connection successful - ready for Nemo API calls");
            } else {
                Serial.println("[SensorApp] WiFi connection failed - Nemo API calls will be skipped");
            }
        } else {
            Serial.println("[SensorApp] No WiFi credentials stored. Sensor data will display without WiFi strength.");
        }
    }

    bool useCelsius = (_units == "C");
    String sensorData = fetchSensorData(useCelsius, wifiConnected);

    // When location is set, use it as the red header line; otherwise use default title
    if (_sensorLocation.length() > 0) {
        int nl = sensorData.indexOf('\n');
        String body = (nl >= 0) ? sensorData.substring(nl + 1) : sensorData;
        sensorData = _sensorLocation + "\n" + body;
    }

    if (_display) {
        renderSensorData(_display, sensorData, batteryPercent);
    }
    if (_display) {
        _display->disableSPI();
    }

    // Optionally POST to Nemo (use raw Celsius readings)
    if (_nemoToken.length() > 0 && _nemoUrl.length() > 0) {
        float tempC, humidity;
        if (getSensorReadingsRaw(tempC, humidity)) {
            postSensorDataToNemo(_nemoUrl.c_str(), _nemoToken.c_str(),
                                 _temperatureSensorId.c_str(), _humiditySensorId.c_str(),
                                 tempC, humidity);
        }
    }

    // Disable WiFi after displaying and posting to save power
    if (_wifi) {
        _wifi->disconnect();
    }

    // Enter deep sleep until next cycle (or delay if no power manager)
    uint32_t sleepSeconds = _refreshIntervalMinutes * 60UL;
    if (_power) {
        Serial.print("[SensorApp] Entering deep sleep for ");
        Serial.print(_refreshIntervalMinutes);
        Serial.println(" min");
        _power->enterDeepSleep(sleepSeconds);
    } else {
        delay(sleepSeconds * 1000UL);
    }
}

void SensorApp::end() {
    Serial.println("[SensorApp] Ending Sensor App");

    if (_display) {
        _display->hibernate();
        _display->disableSPI();
    }
}
