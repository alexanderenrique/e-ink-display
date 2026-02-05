#include "app.h"
#include "fetch.h"
#include "render.h"
#include "../../core/display/display_manager.h"
#include "../../core/power/power_manager.h"

ShelfApp::ShelfApp() {
}

bool ShelfApp::begin() {
    Serial.println("[ShelfApp] Starting Shelf App");
    
    // Initialize display
    if (_display) {
        _display->begin();
    }
    
    return true;
}

void ShelfApp::loop() {
    // Get battery percentage
    int batteryPercent = -1;
    if (_power) {
        batteryPercent = _power->getBatteryPercentage();
    }
    
    // Fetch shelf data
    String shelfData = fetchShelfData();
    
    // Render shelf data
    if (_display) {
        renderShelfData(_display, shelfData, batteryPercent);
    }
    
    // Disable SPI after display update
    if (_display) {
        _display->disableSPI();
    }
    
    // Wait before next cycle (or sleep)
    // For testing: use simple delay
    // TODO: Add refreshIntervalMinutes config from BLE
    uint32_t delayMs = 60000; // 1 minute default
    Serial.print("[ShelfApp] Waiting ");
    Serial.print(delayMs / 1000);
    Serial.print(" seconds (");
    Serial.print(delayMs);
    Serial.println(" ms) before next cycle");
    delay(delayMs);
    
    // Optionally enter deep sleep
    // if (_power) {
    //     // TODO: Use refreshIntervalMinutes from BLE config when available
    //     _power->enterDeepSleep(60); // 1 minute (will use refreshIntervalMinutes * 60 when config is added)
    // }
}

void ShelfApp::end() {
    Serial.println("[ShelfApp] Ending Shelf App");
    
    // Disable display
    if (_display) {
        _display->hibernate();
        _display->disableSPI();
    }
}
