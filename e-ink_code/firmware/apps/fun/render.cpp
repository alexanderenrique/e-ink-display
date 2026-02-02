#include "render.h"
#include "../../core/display/display_manager.h"

void renderDefault(DisplayManager* display, String text, int batteryPercent) {
    if (display == nullptr) return;
    display->displayDefault(text, batteryPercent);
}

void renderEarthquakeFact(DisplayManager* display, String earthquakeData, int batteryPercent) {
    if (display == nullptr) return;
    display->displayEarthquakeFact(earthquakeData, batteryPercent);
}

void renderISSData(DisplayManager* display, String issData, int batteryPercent) {
    if (display == nullptr) return;
    display->displayISSData(issData, batteryPercent);
}
