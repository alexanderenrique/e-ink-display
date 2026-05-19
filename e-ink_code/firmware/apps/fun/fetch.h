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
bool fetchSpecialSlide(FunSlide& out, int displayMode);
/** SNTP when clock looks unset; call before loadHeldSpecialSlide / populating hold deadline. */
void syncFunClockForSpecialHold();
/** Restore a queued special slide from NVS (refresh cycles remaining, optional expiry cap). */
bool loadHeldSpecialSlide(FunSlide& out);
/** Wakes left for the current special slide (0 if none). */
uint8_t specialHoldRefreshCyclesRemaining();
/** While a hold is active, keep rotation on the mode used when the slide was fetched. */
void applySpecialHoldDisplayMode(int& displayMode);
/** Call once per wake after showing a held/fresh special slide; clears after two refresh cycles. */
void consumeSpecialHoldCycle();

#endif  // FUN_APP_FETCH_H
