#include "fetch.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

static Adafruit_SHT31 sht31 = Adafruit_SHT31();
static bool sht31Ready = false;

static String getDateKeyFromCreatedDate(const char* createdDate) {
    // Expected: "YYYY-MM-DDTHH:MM:SS.000000+HH:MM"
    if (createdDate == nullptr) return String();
    if (strlen(createdDate) < 10) return String();
    return String(createdDate).substring(0, 10);
}

static bool hasPostedBatteryForDate(const String& dateKey) {
    if (dateKey.length() == 0) return false;
    Preferences prefs;
    if (!prefs.begin("sensor_app", true)) {
        Serial.println("[SensorApp] Preferences read open failed for battery post date");
        return false;
    }
    String lastDate = prefs.getString("batt_post_date", "");
    prefs.end();
    return lastDate == dateKey;
}

static void setPostedBatteryDate(const String& dateKey) {
    if (dateKey.length() == 0) return;
    Preferences prefs;
    if (!prefs.begin("sensor_app", false)) {
        Serial.println("[SensorApp] Preferences write open failed for battery post date");
        return;
    }
    prefs.putString("batt_post_date", dateKey);
    prefs.end();
}

bool initSensor() {
    Wire.begin(I2C_SDA, I2C_SCL);
    if (sht31.begin(SHT31_I2C_ADDR)) {
        sht31Ready = true;
        Serial.println("[SensorApp] SHT31 initialized");
        return true;
    }
    Serial.println("[SensorApp] SHT31 initialization failed!");
    sht31Ready = false;
    return false;
}

bool getSensorReadingsRaw(float& tempC, float& humidity) {
    if (!sht31Ready) {
        if (!initSensor()) return false;
    }
    tempC = sht31.readTemperature();
    humidity = sht31.readHumidity();
    if (isnan(tempC) || isnan(humidity)) {
        Serial.println("[SensorApp] SHT31 read returned NaN values");
        return false;
    }
    return true;
}

bool getAveragedSensorReadings(float& avgTempC, float& avgHumidity, uint8_t sampleCount) {
    if (sampleCount == 0) return false;

    float tempSum = 0.0f;
    float humiditySum = 0.0f;
    uint8_t validSamples = 0;

    for (uint8_t i = 0; i < sampleCount; ++i) {
        float tempC = 0.0f;
        float humidity = 0.0f;
        if (getSensorReadingsRaw(tempC, humidity)) {
            tempSum += tempC;
            humiditySum += humidity;
            ++validSamples;
        } else {
            Serial.printf("[SensorApp] Sensor sample %u/%u failed\n", i + 1, sampleCount);
        }

        // Small gap between sensor samples to avoid back-to-back reads.
        if (i + 1 < sampleCount) {
            delay(100);
        }
    }

    if (validSamples == 0) {
        Serial.println("[SensorApp] Averaging failed: no valid sensor samples");
        return false;
    }

    avgTempC = tempSum / validSamples;
    avgHumidity = humiditySum / validSamples;
    Serial.printf("[SensorApp] Averaged %u/%u samples: %.2f C, %.2f %%RH\n",
                  validSamples, sampleCount, avgTempC, avgHumidity);
    return true;
}

static String buildDisplayStringFromReadings(float tempC, float humidity, bool useCelsius, bool wifiConnected, String lastUpdatedTime) {
    float displayTemp = useCelsius ? tempC : (tempC * 9.0f / 5.0f + 32.0f);
    const char* unitStr = useCelsius ? "°C" : "°F";

    String result = String("Temperature & Humidity\n");
    result += String("Temp: ") + String(displayTemp, 1) + unitStr + "\n";
    result += String("Humidity: ") + String(humidity, 1) + "%";

    if (wifiConnected && WiFi.status() == WL_CONNECTED) {
        int rssi = WiFi.RSSI();
        String strengthDesc;
        if (rssi > -50) {
            strengthDesc = "Excellent";
        } else if (rssi >= -60 && rssi <= -50) {
            strengthDesc = "Great";
        } else if (rssi >= -70 && rssi < -60) {
            strengthDesc = "Good";
        } else if (rssi >= -80 && rssi < -70) {
            strengthDesc = "Fair";
        } else if (rssi >= -90 && rssi < -80) {
            strengthDesc = "Weak";
        } else {
            strengthDesc = "Very Poor";
        }
        result += String("\nWiFi: ") + String(rssi) + " dBm (" + strengthDesc + ")";
        if (lastUpdatedTime.length() > 0) {
            result += String("\nUpdated: ") + lastUpdatedTime;
        } else {
            result += String("\nUpdated: --");
        }
    }

    return result;
}

String formatSensorDataForDisplay(float tempC, float humidity, bool useCelsius, bool wifiConnected, String lastUpdatedTime) {
    return buildDisplayStringFromReadings(tempC, humidity, useCelsius, wifiConnected, lastUpdatedTime);
}

String fetchSensorData(bool useCelsius, bool wifiConnected, String lastUpdatedTime) {
    float tempC, humidity;
    if (!getSensorReadingsRaw(tempC, humidity)) {
        return "Sensor Error\nRead failed";
    }
    return buildDisplayStringFromReadings(tempC, humidity, useCelsius, wifiConnected, lastUpdatedTime);
}

void setTimezoneRule(const char* tzRule) {
    if (tzRule == nullptr || *tzRule == '\0') return;
    setenv("TZ", tzRule, 1);
    tzset();
}

String getLocalTimeForDisplay(const char* strftimeFormat) {
    if (strftimeFormat == nullptr || *strftimeFormat == '\0') {
        strftimeFormat = "%Y-%m-%d %H:%M";
    }
    time_t now = time(nullptr);
    if (now <= 0) return String();
    struct tm timeinfo;
    if (!localtime_r(&now, &timeinfo)) return String();
    char buf[48];
    size_t n = strftime(buf, sizeof(buf), strftimeFormat, &timeinfo);
    if (n == 0) return String();
    return String(buf);
}

bool syncTimeFromNtp(const char* ntpServer) {
    if (ntpServer == nullptr || *ntpServer == '\0') return false;
    // Always sync in UTC; localtime() will use TZ rules for local display/DST.
    configTime(0, 0, ntpServer);
    for (int i = 0; i < 50; i++) {
        time_t now = time(nullptr);
        if (now > 0) {
            Serial.println("[SensorApp] NTP time synced");
            Serial.print("[SensorApp] NTP epoch: ");
            Serial.println((long)now);
            struct tm utc;
            gmtime_r(&now, &utc);
            char buf[40];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &utc);
            Serial.print("[SensorApp] NTP time (UTC): ");
            Serial.println(buf);
            return true;
        }
        delay(100);
    }
    Serial.println("[SensorApp] NTP time sync failed (timeout)");
    return false;
}

static String formatIso8601OffsetFromZ(const char* z) {
    if (z == nullptr) return String();
    // Expected forms: "+HHMM" or "-HHMM"
    if (strlen(z) != 5) return String(z);
    String out;
    out.reserve(6);
    out += z[0];
    out += z[1];
    out += z[2];
    out += ":";
    out += z[3];
    out += z[4];
    return out;
}

String getIso8601CreatedDate() {
    time_t now = time(nullptr);
    if (now <= 0) return String();
    struct tm timeinfo;
    if (!localtime_r(&now, &timeinfo)) return String();
    char dt[32];
    char z[8];
    if (strftime(dt, sizeof(dt), "%Y-%m-%dT%H:%M:%S", &timeinfo) == 0) return String();
    if (strftime(z, sizeof(z), "%z", &timeinfo) == 0) return String();
    return String(dt) + ".000000" + formatIso8601OffsetFromZ(z);
}

bool postSensorDataToNemo(const char* url, const char* token,
                          const char* temperatureSensorId, const char* humiditySensorId, const char* batterySensorId,
                          float tempC, float humidity, int batteryPercent, const char* createdDate) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[SensorApp] Nemo POST skipped: WiFi not connected");
        return false;
    }
    if (url == nullptr || *url == '\0' || token == nullptr) {
        Serial.println("[SensorApp] Nemo POST skipped: missing url/token");
        return false;
    }
    if (createdDate == nullptr || *createdDate == '\0') {
        Serial.println("[SensorApp] Nemo POST skipped: created_date required (sync time first)");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();  // Accept any HTTPS cert; use setCACert() for production if needed

    bool success = true;

    // POST temperature reading if sensor ID is provided
    if (temperatureSensorId != nullptr && *temperatureSensorId != '\0') {
        HTTPClient http;
        if (!http.begin(client, url)) {
            Serial.println("[SensorApp] Nemo POST (temp): http.begin failed");
            success = false;
        } else {
            http.addHeader("Content-Type", "application/json");
            http.addHeader("Authorization", String("Token ") + token);

            DynamicJsonDocument doc(256);
            doc["sensor"] = atoi(temperatureSensorId);  // Convert string to int
            doc["value"] = roundf(tempC * 10.0f) / 10.0f;
            doc["created_date"] = createdDate;

            String body;
            serializeJson(doc, body);

            int httpCode = http.POST(body);
            String responseBody;
            if (httpCode > 0) {
                responseBody = http.getString();
            }

            if (httpCode > 0 && httpCode < 300) {
                Serial.printf("[SensorApp] Nemo POST (temp) OK: %d\n", httpCode);
            } else {
                Serial.printf("[SensorApp] Nemo POST (temp) failed: %d %s\n", httpCode,
                              httpCode > 0 ? responseBody.c_str() : http.errorToString(httpCode).c_str());
                success = false;
            }
            if (httpCode > 0) {
                Serial.print("[SensorApp] Nemo POST (temp) response body: ");
                Serial.println(responseBody);
            }
            http.end();
        }
    }

    // POST humidity reading if sensor ID is provided
    if (humiditySensorId != nullptr && *humiditySensorId != '\0') {
        HTTPClient http;
        if (!http.begin(client, url)) {
            Serial.println("[SensorApp] Nemo POST (humidity): http.begin failed");
            success = false;
        } else {
            http.addHeader("Content-Type", "application/json");
            http.addHeader("Authorization", String("Token ") + token);

            DynamicJsonDocument doc(256);
            doc["sensor"] = atoi(humiditySensorId);  // Convert string to int
            doc["value"] = roundf(humidity * 10.0f) / 10.0f;
            doc["created_date"] = createdDate;

            String body;
            serializeJson(doc, body);

            int httpCode = http.POST(body);
            String responseBody;
            if (httpCode > 0) {
                responseBody = http.getString();
            }

            if (httpCode > 0 && httpCode < 300) {
                Serial.printf("[SensorApp] Nemo POST (humidity) OK: %d\n", httpCode);
            } else {
                Serial.printf("[SensorApp] Nemo POST (humidity) failed: %d %s\n", httpCode,
                              httpCode > 0 ? responseBody.c_str() : http.errorToString(httpCode).c_str());
                success = false;
            }
            if (httpCode > 0) {
                Serial.print("[SensorApp] Nemo POST (humidity) response body: ");
                Serial.println(responseBody);
            }
            http.end();
        }
    }

    // POST battery reading if sensor ID is provided and battery reading is valid
    if (batterySensorId != nullptr && *batterySensorId != '\0') {
        if (batteryPercent < 0) {
            Serial.println("[SensorApp] Nemo POST (battery) skipped: invalid battery reading");
        } else {
            String batteryDateKey = getDateKeyFromCreatedDate(createdDate);
            if (hasPostedBatteryForDate(batteryDateKey)) {
                Serial.print("[SensorApp] Nemo POST (battery) skipped: already posted for ");
                Serial.println(batteryDateKey);
            } else {
                HTTPClient http;
                if (!http.begin(client, url)) {
                    Serial.println("[SensorApp] Nemo POST (battery): http.begin failed");
                    success = false;
                } else {
                    http.addHeader("Content-Type", "application/json");
                    http.addHeader("Authorization", String("Token ") + token);

                    DynamicJsonDocument doc(256);
                    doc["sensor"] = atoi(batterySensorId);  // Convert string to int
                    doc["value"] = batteryPercent;
                    doc["created_date"] = createdDate;

                    String body;
                    serializeJson(doc, body);

                    int httpCode = http.POST(body);
                    String responseBody;
                    if (httpCode > 0) {
                        responseBody = http.getString();
                    }

                    if (httpCode > 0 && httpCode < 300) {
                        Serial.printf("[SensorApp] Nemo POST (battery) OK: %d\n", httpCode);
                        setPostedBatteryDate(batteryDateKey);
                    } else {
                        Serial.printf("[SensorApp] Nemo POST (battery) failed: %d %s\n", httpCode,
                                      httpCode > 0 ? responseBody.c_str() : http.errorToString(httpCode).c_str());
                        success = false;
                    }
                    if (httpCode > 0) {
                        Serial.print("[SensorApp] Nemo POST (battery) response body: ");
                        Serial.println(responseBody);
                    }
                    http.end();
                }
            }
        }
    }

    return success;
}
