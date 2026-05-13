#ifndef FUN_APP_FETCH_H
#define FUN_APP_FETCH_H

#include "fun_slide.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

void initI2C();
String getRoomData();

bool fetchFunScreenSlide(int mode, FunSlide& out);
bool fetchMixedFunSlide(FunSlide& out);
/** When the server has a queued slide for this device's X-Device-Id, fills ``out`` (dequeued). */
bool fetchSpecialSlide(FunSlide& out);
/** SNTP when clock looks unset; call before loadHeldSpecialSlide / populating hold deadline. */
void syncFunClockForSpecialHold();
/** Restore a queued special slide from NVS while within display_hold_until_epoch (UTC). */
bool loadHeldSpecialSlide(FunSlide& out);

#endif  // FUN_APP_FETCH_H
