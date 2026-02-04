#include "fetch.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

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

String fetchSensorData(bool useCelsius) {
    float tempC, humidity;
    if (!getSensorReadingsRaw(tempC, humidity)) {
        return "Sensor Error\nRead failed";
    }

    float displayTemp = useCelsius ? tempC : (tempC * 9.0f / 5.0f + 32.0f);
    const char* unitStr = useCelsius ? "°C" : "°F";

    String result = String("Temperature & Humidity\n");
    result += String("Temp: ") + String(displayTemp, 1) + unitStr + "\n";
    result += String("Humidity: ") + String(humidity, 1) + "%";
    return result;
}

bool postSensorDataToNemo(const char* url, const char* token, const char* sensorId,
                          float tempC, float humidity) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[SensorApp] Nemo POST skipped: WiFi not connected");
        return false;
    }
    if (url == nullptr || *url == '\0' || token == nullptr || sensorId == nullptr) {
        Serial.println("[SensorApp] Nemo POST skipped: missing url/token/sensorId");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();  // Accept any HTTPS cert; use setCACert() for production if needed

    HTTPClient http;
    if (!http.begin(client, url)) {
        Serial.println("[SensorApp] Nemo POST: http.begin failed");
        return false;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Token ") + token);

    DynamicJsonDocument doc(256);
    doc["sensor_id"] = sensorId;
    doc["temperature"] = roundf(tempC * 10.0f) / 10.0f;
    doc["humidity"] = roundf(humidity * 10.0f) / 10.0f;

    String body;
    serializeJson(doc, body);

    int httpCode = http.POST(body);

    if (httpCode > 0 && httpCode < 300) {
        Serial.printf("[SensorApp] Nemo POST OK: %d\n", httpCode);
        http.end();
        return true;
    }
    Serial.printf("[SensorApp] Nemo POST failed: %d %s\n", httpCode,
                  httpCode > 0 ? http.getString().c_str() : http.errorToString(httpCode).c_str());
    http.end();
    return false;
}
