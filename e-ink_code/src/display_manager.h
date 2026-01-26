#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>

// Forward declaration - display object is defined in main.cpp
extern GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display;

// Display functions
void displayDefault(String fact);
void displayEarthquakeFact(String earthquakeData);
void displayISSData(String issData);
void displayFact(String fact);

// Helper functions
int renderTextWithWrap(String text, int startX, int startY, int maxWidth, int lineHeight, uint16_t textColor);
void displayBatteryPercentage();

#endif // DISPLAY_MANAGER_H
