#include "fun_slide.h"

bool funSlideFromJson(JsonObjectConst obj, FunSlide& out) {
    if (!obj.containsKey("text")) {
        return false;
    }
    out.displayHoldUntilEpoch = 0;
    out.text = obj["text"].as<String>();
    if (obj.containsKey("layout")) {
        out.layout = obj["layout"].as<String>();
    } else {
        out.layout = "default";
    }
    out.layout.trim();
    out.layout.toLowerCase();
    if (!obj["display_hold_until_epoch"].isNull()) {
        uint64_t uh = obj["display_hold_until_epoch"].as<uint64_t>();
        if (uh <= 0xffffffffULL) {
            out.displayHoldUntilEpoch = static_cast<uint32_t>(uh);
        }
    }
    return out.text.length() > 0;
}
