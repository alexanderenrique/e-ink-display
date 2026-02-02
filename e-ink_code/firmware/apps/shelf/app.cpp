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
    
    // Wait before next cycle
    delay(60000); // 1 minute
}

void ShelfApp::end() {
    Serial.println("[ShelfApp] Ending Shelf App");
    
    // Disable display
    if (_display) {
        _display->hibernate();
        _display->disableSPI();
    }
}
