#ifndef FUN_SLIDE_H
#define FUN_SLIDE_H

#include <Arduino.h>
#include <ArduinoJson.h>

struct FunSlide {
    String layout;
    String text;
    /** Optional UNIX epoch UTC cap from GET /v1/fun/special (message expires_at); not hold duration. */
    uint32_t displayHoldUntilEpoch = 0;
};

bool funSlideFromJson(JsonObjectConst obj, FunSlide& out);

#endif
