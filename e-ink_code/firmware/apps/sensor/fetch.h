#ifndef SENSOR_APP_FETCH_H
#define SENSOR_APP_FETCH_H

#include <Arduino.h>

// Initialize I2C and SHT31 sensor. Call from app begin(). Returns true if sensor is ready.
bool initSensor();

// Read temperature and humidity; return formatted string for display.
// useCelsius: true = °C, false = °F
// wifiConnected: if true, WiFi strength will be included in the display string
String fetchSensorData(bool useCelsius = false, bool wifiConnected = false);

// Raw readings in Celsius (for Nemo API). Returns true if read succeeded.
bool getSensorReadingsRaw(float& tempC, float& humidity);

// POST sensor data to Nemo API. Uses WiFi (must be connected). Returns true on HTTP 2xx.
// Makes two separate POST requests: one for temperature, one for humidity.
bool postSensorDataToNemo(const char* url, const char* token, 
                          const char* temperatureSensorId, const char* humiditySensorId,
                          float tempC, float humidity);

#endif // SENSOR_APP_FETCH_H
