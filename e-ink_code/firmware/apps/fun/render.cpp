#include "render.h"
#include "fun_slide.h"
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

void renderFunSlide(DisplayManager* display, const FunSlide& slide, int batteryPercent) {
    if (display == nullptr) return;
    String l = slide.layout;
    l.toLowerCase();
    if (l == "earthquake") {
        display->displayEarthquakeFact(slide.text, batteryPercent);
    } else if (l == "iss") {
        display->displayISSData(slide.text, batteryPercent);
    } else {
        display->displayDefault(slide.text, batteryPercent);
    }
}
