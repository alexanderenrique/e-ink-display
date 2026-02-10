#ifndef MESSAGES_APP_RENDER_H
#define MESSAGES_APP_RENDER_H

#include <Arduino.h>

class DisplayManager;

void renderMessages(DisplayManager* display, const String& text, int batteryPercent);

#endif // MESSAGES_APP_RENDER_H
