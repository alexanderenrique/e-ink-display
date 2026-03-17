#ifndef SENSOR_APP_FETCH_H
#define SENSOR_APP_FETCH_H

#include <Arduino.h>

// Initialize I2C and SHT31 sensor. Call from app begin(). Returns true if sensor is ready.
bool initSensor();

// Read temperature and humidity; return formatted string for display.
// useCelsius: true = °C, false = °F
// wifiConnected: if true, WiFi strength will be included in the display string
// lastUpdatedTime: if non-empty, will be displayed below WiFi status
String fetchSensorData(bool useCelsius = false, bool wifiConnected = false, String lastUpdatedTime = "");

// Format already-read temp (C) and humidity for display (no I2C read). Use after a single getSensorReadingsRaw().
String formatSensorDataForDisplay(float tempC, float humidity, bool useCelsius, bool wifiConnected, String lastUpdatedTime);

// Raw readings in Celsius (for Nemo API). Returns true if read succeeded.
bool getSensorReadingsRaw(float& tempC, float& humidity);

// Sync system time from NTP. Call when WiFi is connected. Returns true when time is set.
bool syncTimeFromNtp(const char* ntpServer);

// Apply a POSIX TZ rule string (for DST-aware localtime()).
void setTimezoneRule(const char* tzRule);

// Returns local time formatted for display (empty if time not set).
String getLocalTimeForDisplay(const char* strftimeFormat = "%Y-%m-%d %H:%M");

// Get current local time as ISO 8601 string for Nemo created_date
// (e.g. "2026-03-02T06:04:04.000000-08:00"). Returns empty string if time not set.
String getIso8601CreatedDate();

// POST sensor data to Nemo API. Uses WiFi (must be connected). Returns true on HTTP 2xx.
// Payload: sensor (id), value, created_date (ISO 8601). Pass from getIso8601CreatedDate() after syncTimeFromNtp().
bool postSensorDataToNemo(const char* url, const char* token,
                          const char* temperatureSensorId, const char* humiditySensorId,
                          float tempC, float humidity, const char* createdDate);

#endif // SENSOR_APP_FETCH_H
