#include "app.h"
#include "fetch.h"
#include "render.h"
#include "../../core/display/display_manager.h"
#include "../../core/power/power_manager.h"

SensorApp::SensorApp() {
}

bool SensorApp::begin() {
    Serial.println("[SensorApp] Starting Sensor App");
    
    // Initialize display
    if (_display) {
        _display->begin();
    }
    
    return true;
}

void SensorApp::loop() {
    // Get battery percentage
    int batteryPercent = -1;
    if (_power) {
        batteryPercent = _power->getBatteryPercentage();
    }
    
    // Fetch sensor data
    String sensorData = fetchSensorData();
    
    // Render sensor data
    if (_display) {
        renderSensorData(_display, sensorData, batteryPercent);
    }
    
    // Disable SPI after display update
    if (_display) {
        _display->disableSPI();
    }
    
    // Wait before next cycle
    delay(60000); // 1 minute
}

void SensorApp::end() {
    Serial.println("[SensorApp] Ending Sensor App");
    
    // Disable display
    if (_display) {
        _display->hibernate();
        _display->disableSPI();
    }
}
