#ifndef FUN_APP_RENDER_H
#define FUN_APP_RENDER_H

#include <Arduino.h>

// Forward declaration
class DisplayManager;

// Render functions that use DisplayManager
void renderDefault(DisplayManager* display, String text, int batteryPercent);
void renderEarthquakeFact(DisplayManager* display, String earthquakeData, int batteryPercent);
void renderISSData(DisplayManager* display, String issData, int batteryPercent);

#endif // FUN_APP_RENDER_H
