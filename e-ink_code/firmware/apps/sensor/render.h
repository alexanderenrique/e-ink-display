#ifndef SENSOR_APP_RENDER_H
#define SENSOR_APP_RENDER_H

#include <Arduino.h>

// Forward declaration
class DisplayManager;

// Render functions
void renderSensorData(DisplayManager* display, String data, int batteryPercent);

#endif // SENSOR_APP_RENDER_H
