#ifndef SHELF_APP_RENDER_H
#define SHELF_APP_RENDER_H

#include <Arduino.h>

// Forward declaration
class DisplayManager;

// Render functions
void renderShelfData(DisplayManager* display, String data, int batteryPercent);

#endif // SHELF_APP_RENDER_H
