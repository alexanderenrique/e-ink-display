#include "render.h"
#include "../../core/display/display_manager.h"

void renderSensorData(DisplayManager* display, String data, int batteryPercent) {
    if (display == nullptr) return;
    display->displayDefault(data, batteryPercent);
}
