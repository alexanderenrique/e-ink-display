#include "render.h"
#include "../../core/display/display_manager.h"

void renderMessages(DisplayManager* display, const String& text, int batteryPercent) {
    if (display == nullptr) return;
    display->displayTextOnly(text, batteryPercent);
}
