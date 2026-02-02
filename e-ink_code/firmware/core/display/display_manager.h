#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <SPI.h>
#include "hardware_config.h"

// GxEPD2_290_C90c is for GDEM029C90 128x296 3-color display
extern GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display;

class DisplayManager {
public:
    DisplayManager();
    bool begin();
    void hibernate();
    
    // Display functions
    void displayDefault(String text, int batteryPercent = -1);
    void displayEarthquakeFact(String earthquakeData, int batteryPercent = -1);
    void displayISSData(String issData, int batteryPercent = -1);
    
    // Helper functions
    int renderTextWithWrap(String text, int startX, int startY, int maxWidth, int lineHeight, uint16_t textColor);
    void displayBatteryPercentage(int batteryPercent);
    
    // SPI management
    void initSPI();
    void disableSPI();

private:
    bool _initialized;
};

#endif // DISPLAY_MANAGER_H
