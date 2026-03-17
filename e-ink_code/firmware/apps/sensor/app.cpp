#include "app.h"
#include "fetch.h"
#include "render.h"
#include "../../core/display/display_manager.h"
#include "../../core/power/power_manager.h"
#include "../../core/bluetooth/cold_start_ble.h"
#include "../../core/wifi/wifi_manager.h"
#include "../../core/hardware_config.h"

SensorApp::SensorApp() {
}

static const char* tzRuleForSelection(const String& selection) {
    if (selection == SENSOR_APP_TIMEZONE_PACIFIC) return SENSOR_APP_TZ_RULE_PACIFIC;
    if (selection == SENSOR_APP_TIMEZONE_MOUNTAIN) return SENSOR_APP_TZ_RULE_MOUNTAIN;
    if (selection == SENSOR_APP_TIMEZONE_CENTRAL) return SENSOR_APP_TZ_RULE_CENTRAL;
    if (selection == SENSOR_APP_TIMEZONE_EASTERN) return SENSOR_APP_TZ_RULE_EASTERN;
    if (selection == SENSOR_APP_TIMEZONE_ARIZONA) return SENSOR_APP_TZ_RULE_ARIZONA;
    if (selection == SENSOR_APP_TIMEZONE_UTC) return SENSOR_APP_TZ_RULE_UTC;
    return SENSOR_APP_DEFAULT_TZ_RULE;
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

    if (config.containsKey("timeServer")) {
        _timeServer = config["timeServer"].as<const char*>();
    } else if (config.containsKey("time_server")) {
        _timeServer = config["time_server"].as<const char*>();
    }
    // New timezone selection (preferred)
    if (config.containsKey("timeZone")) {
        _timeZone = config["timeZone"].as<const char*>();
    } else if (config.containsKey("timezone")) {
        _timeZone = config["timezone"].as<const char*>();
    } else if (config.containsKey("time_zone")) {
        _timeZone = config["time_zone"].as<const char*>();
    }
    _timeZone.toLowerCase();
    _tzRule = tzRuleForSelection(_timeZone);
    Serial.print("[SensorApp] Timezone selection: ");
    Serial.println(_timeZone.length() > 0 ? _timeZone : String(SENSOR_APP_DEFAULT_TIMEZONE));

    // Legacy config support (kept for backwards compatibility; no longer used for local time)
    if (config.containsKey("gmtOffsetSec")) _gmtOffsetSec = config["gmtOffsetSec"].as<long>();
    if (config.containsKey("daylightOffsetSec")) _daylightOffsetSec = config["daylightOffsetSec"].as<int>();

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
    // Power on display and temperature sensor (MOSFET on pin 20; LOW = on)
    pinMode(POWER_DISPLAY_SENSOR_PIN, OUTPUT);
    digitalWrite(POWER_DISPLAY_SENSOR_PIN, LOW);

    int batteryPercent = -1;
    if (_power) {
        batteryPercent = _power->getBatteryPercentage();
    }

    // Try to connect WiFi to show WiFi strength if available
    // Also connect if we have Nemo config for posting data
    bool wifiConnected = false;
    bool timeSynced = false;
    String lastUpdatedTime;
    if (_wifi) {
        String wifiSSID = ColdStartBle::getStoredWiFiSSID();
        String wifiPassword = ColdStartBle::getStoredWiFiPassword();
        if (wifiSSID.length() > 0) {
            Serial.println("[SensorApp] WiFi connection requested");
            wifiConnected = _wifi->begin(wifiSSID.c_str(), wifiPassword.c_str());
            if (wifiConnected) {
                Serial.println("[SensorApp] WiFi connection successful - ready for Nemo API calls");
                setTimezoneRule(_tzRule.c_str());
                Serial.print("[SensorApp] Time server: ");
                Serial.println(_timeServer);
                Serial.print("[SensorApp] TZ rule: ");
                Serial.println(_tzRule);
                timeSynced = syncTimeFromNtp(_timeServer.c_str());
                if (timeSynced) {
                    // Re-apply TZ after NTP sync; some ESP32 configTime() paths can leave TZ unapplied for the first time() use
                    setTimezoneRule(_tzRule.c_str());
                    lastUpdatedTime = getLocalTimeForDisplay("%m/%d %H:%M");
                    Serial.print("[SensorApp] Time displayed on e-ink: ");
                    Serial.println(lastUpdatedTime);
                    time_t now = time(nullptr);
                    Serial.print("[SensorApp] Epoch when building display time: ");
                    Serial.println((long)now);
                }
            } else {
                Serial.println("[SensorApp] WiFi connection failed - Nemo API calls will be skipped");
            }
        } else {
            Serial.println("[SensorApp] No WiFi credentials stored. Sensor data will display without WiFi strength.");
        }
    }

    // Single I2C read: use for both display and Nemo. Second read after display/SPI disable often fails (I2C -1).
    bool useCelsius = (_units == "C");
    float tempC = 0.0f;
    float humidity = 0.0f;
    bool readOk = getSensorReadingsRaw(tempC, humidity);
    String sensorData = readOk
        ? formatSensorDataForDisplay(tempC, humidity, useCelsius, wifiConnected, lastUpdatedTime)
        : "Sensor Error\nRead failed";

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

    // Optionally POST to Nemo using the same readings (no second I2C read)
    if (_nemoToken.length() > 0 && _nemoUrl.length() > 0) {
        if (!readOk) {
            Serial.println("[SensorApp] Nemo POST skipped: sensor read failed earlier");
        } else if (!wifiConnected) {
            Serial.println("[SensorApp] Nemo POST skipped: WiFi required for time sync and upload");
        } else {
            setTimezoneRule(_tzRule.c_str());
            if (!timeSynced && !syncTimeFromNtp(_timeServer.c_str())) {
                Serial.println("[SensorApp] Time sync failed before Nemo POST");
            } else {
                String createdDate = getIso8601CreatedDate();
                if (createdDate.length() > 0) {
                    Serial.println("[SensorApp] Nemo POST: calling postSensorDataToNemo");
                    postSensorDataToNemo(_nemoUrl.c_str(), _nemoToken.c_str(),
                                        _temperatureSensorId.c_str(), _humiditySensorId.c_str(),
                                        tempC, humidity, createdDate.c_str());
                } else {
                    Serial.println("[SensorApp] Nemo POST skipped: could not get time for created_date");
                }
            }
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
