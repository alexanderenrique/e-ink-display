#include "fetch.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <WiFi.h>

static Adafruit_SHT31 sht31 = Adafruit_SHT31();
static bool sht31Ready = false;

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
    if (isnan(tempC) || isnan(humidity)) return false;
    return true;
}

String fetchSensorData(bool useCelsius, bool wifiConnected) {
    float tempC, humidity;
    if (!getSensorReadingsRaw(tempC, humidity)) {
        return "Sensor Error\nRead failed";
    }

    float displayTemp = useCelsius ? tempC : (tempC * 9.0f / 5.0f + 32.0f);
    const char* unitStr = useCelsius ? "°C" : "°F";

    String result = String("Temperature & Humidity\n");
    result += String("Temp: ") + String(displayTemp, 1) + unitStr + "\n";
    result += String("Humidity: ") + String(humidity, 1) + "%";
    
    // Add WiFi info if connected
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
    }
    
    return result;
}

bool postSensorDataToNemo(const char* url, const char* token, 
                          const char* temperatureSensorId, const char* humiditySensorId,
                          float tempC, float humidity) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[SensorApp] Nemo POST skipped: WiFi not connected");
        return false;
    }
    if (url == nullptr || *url == '\0' || token == nullptr) {
        Serial.println("[SensorApp] Nemo POST skipped: missing url/token");
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

            DynamicJsonDocument doc(128);
            doc["sensor"] = atoi(temperatureSensorId);  // Convert string to int
            doc["value"] = roundf(tempC * 10.0f) / 10.0f;

            String body;
            serializeJson(doc, body);

            int httpCode = http.POST(body);

            if (httpCode > 0 && httpCode < 300) {
                Serial.printf("[SensorApp] Nemo POST (temp) OK: %d\n", httpCode);
            } else {
                Serial.printf("[SensorApp] Nemo POST (temp) failed: %d %s\n", httpCode,
                              httpCode > 0 ? http.getString().c_str() : http.errorToString(httpCode).c_str());
                success = false;
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

            DynamicJsonDocument doc(128);
            doc["sensor"] = atoi(humiditySensorId);  // Convert string to int
            doc["value"] = roundf(humidity * 10.0f) / 10.0f;

            String body;
            serializeJson(doc, body);

            int httpCode = http.POST(body);

            if (httpCode > 0 && httpCode < 300) {
                Serial.printf("[SensorApp] Nemo POST (humidity) OK: %d\n", httpCode);
            } else {
                Serial.printf("[SensorApp] Nemo POST (humidity) failed: %d %s\n", httpCode,
                              httpCode > 0 ? http.getString().c_str() : http.errorToString(httpCode).c_str());
                success = false;
            }
            http.end();
        }
    }

    return success;
}
