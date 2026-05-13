#ifndef FUN_APP_RENDER_H
#define FUN_APP_RENDER_H

#include <Arduino.h>

#include "fun_slide.h"

class DisplayManager;

void renderDefault(DisplayManager* display, String text, int batteryPercent);
void renderEarthquakeFact(DisplayManager* display, String earthquakeData, int batteryPercent);
void renderISSData(DisplayManager* display, String issData, int batteryPercent);
void renderFunSlide(DisplayManager* display, const FunSlide& slide, int batteryPercent);

#endif  // FUN_APP_RENDER_H
