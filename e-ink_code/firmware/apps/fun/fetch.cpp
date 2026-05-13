#include "fetch.h"
#include "config.h"
#include "../../core/bluetooth/cold_start_ble.h"
#include <Adafruit_SHT31.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Wire.h>
#include <time.h>

Adafruit_SHT31 sht31 = Adafruit_SHT31();

static constexpr time_t kMinValidUtcEpoch = 1577836800;

static constexpr const char* kSpecialHoldNs = "fun_sp";
static constexpr const char* kHoldKeyUntil = "u";
static constexpr const char* kHoldKeyText = "t";
static constexpr const char* kHoldKeyLayout = "l";

static bool utcClockProbablyValid() {
    return time(nullptr) > kMinValidUtcEpoch;
}

static void clearSpecialHoldPrefs() {
    Preferences prefs;
    if (prefs.begin(kSpecialHoldNs, false)) {
        prefs.clear();
        prefs.end();
    }
}

static void persistSpecialHold(const FunSlide& slide) {
    if (slide.displayHoldUntilEpoch == 0 || slide.text.length() == 0) {
        return;
    }
    Preferences prefs;
    if (!prefs.begin(kSpecialHoldNs, false)) {
        return;
    }
    prefs.putUInt(kHoldKeyUntil, slide.displayHoldUntilEpoch);
    prefs.putString(kHoldKeyText, slide.text);
    prefs.putString(kHoldKeyLayout, slide.layout.length() > 0 ? slide.layout : String("default"));
    prefs.end();
}

namespace {

constexpr size_t kMaxFriendlyChars = 160;

/** Obtain server-assigned device_id when NVS slot is empty; phone-mint id skips POST. */
static bool ensureRegisteredWithFunServer() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    String deviceId = ColdStartBle::getStoredDeviceId();
    if (deviceId.length() > 0) {
        return true;
    }

    String url = String(FUN_FACTS_BASE_URL) + "/v1/devices/register";
    DynamicJsonDocument bodyDoc(512);
    String friendly = ColdStartBle::getStoredFriendlyName();
    if (friendly.length() == 0) {
        friendly = String("eink-") + WiFi.macAddress();
        friendly.replace(":", "");
    }
    if (friendly.length() > kMaxFriendlyChars) {
        friendly = friendly.substring(0, kMaxFriendlyChars);
    }
    bodyDoc["friendly_name"] = friendly;
    bodyDoc["hardware_mac"] = WiFi.macAddress();

    HTTPClient http;
    if (!http.begin(url)) {
        Serial.println("[FunFetch] register: http.begin failed");
        return false;
    }
    if (strlen(FUN_FACTS_API_KEY) > 0) {
        http.addHeader("X-Fun-Key", FUN_FACTS_API_KEY);
    }
    http.addHeader("Content-Type", "application/json");

    String bodyStr;
    serializeJson(bodyDoc, bodyStr);

    int httpCode = http.POST(bodyStr);
    String payload = http.getString();
    http.end();

    if (httpCode <= 0) {
        Serial.printf("[FunFetch] register HTTP failed: %s\n", http.errorToString(httpCode).c_str());
        return false;
    }
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[FunFetch] register HTTP status=%d body=%s\n", httpCode, payload.c_str());
        return false;
    }
    payload.trim();

    DynamicJsonDocument resDoc(384);
    DeserializationError err = deserializeJson(resDoc, payload);
    if (err) {
        Serial.print("[FunFetch] register JSON error: ");
        Serial.println(err.c_str());
        return false;
    }
    const char* newId = nullptr;
    if (resDoc.containsKey("device_id")) {
        newId = resDoc["device_id"].as<const char*>();
    } else if (resDoc.containsKey("deviceId")) {
        newId = resDoc["deviceId"].as<const char*>();
    }
    if (newId == nullptr || strlen(newId) == 0) {
        Serial.println("[FunFetch] register: missing device_id in response");
        return false;
    }

    ColdStartBle::putStoredDeviceId(String(newId));
    Serial.println("[FunFetch] Registered with fun server");
    return true;
}

static void addFunHeaders(HTTPClient& http) {
    if (strlen(FUN_FACTS_API_KEY) > 0) {
        http.addHeader("X-Fun-Key", FUN_FACTS_API_KEY);
    }
    String did = ColdStartBle::getStoredDeviceId();
    if (did.length() > 0) {
        http.addHeader("X-Device-Id", did);
    }
    String fname = ColdStartBle::getStoredFriendlyName();
    if (fname.length() > 0) {
        http.addHeader("X-Device-Name", fname);
    }
}

}  // namespace

void syncFunClockForSpecialHold() {
    if (utcClockProbablyValid()) {
        return;
    }
    configTime(0, 0, "pool.ntp.org");
    for (int i = 0; i < 45; ++i) {
        if (time(nullptr) > kMinValidUtcEpoch) {
            return;
        }
        delay(100);
    }
}

bool loadHeldSpecialSlide(FunSlide& out) {
    if (!utcClockProbablyValid()) {
        return false;
    }
    time_t nowSecs = time(nullptr);
    Preferences prefs;
    if (!prefs.begin(kSpecialHoldNs, true)) {
        return false;
    }
    uint32_t until = prefs.getUInt(kHoldKeyUntil, 0);
    String txt = prefs.getString(kHoldKeyText, "");
    String lay = prefs.getString(kHoldKeyLayout, "default");
    prefs.end();

    if (until == 0 || txt.length() == 0) {
        return false;
    }

    uint32_t nowU = static_cast<uint32_t>(nowSecs);
    if (nowU >= until) {
        clearSpecialHoldPrefs();
        return false;
    }

    out.text = txt;
    out.layout = lay;
    out.layout.trim();
    out.layout.toLowerCase();
    out.displayHoldUntilEpoch = until;
    return true;
}

void initI2C() {
    Wire.begin(I2C_SDA, I2C_SCL);

    if (!sht31.begin(0x44)) {
        Serial.println("SHT31 sensor initialization failed!");
    }
}

bool fetchFunScreenSlide(int mode, FunSlide& out) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected!");
        return false;
    }

    ensureRegisteredWithFunServer();

    HTTPClient http;
    String url = String(FUN_FACTS_BASE_URL) + "/v1/fun/screen?m=" + String(mode);
    if (!http.begin(url)) {
        return false;
    }
    addFunHeaders(http);

    int httpCode = http.GET();
    bool ok = false;
    if (httpCode > 0) {
        String payload = http.getString();
        payload.trim();
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, payload);
        if (!error && doc.is<JsonObject>()) {
            ok = funSlideFromJson(doc.as<JsonObjectConst>(), out);
        } else if (error) {
            Serial.print("JSON parse error: ");
            Serial.println(error.c_str());
        }
    } else {
        Serial.printf("HTTP screen failed: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
    return ok;
}

bool fetchMixedFunSlide(FunSlide& out) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected!");
        return false;
    }

    ensureRegisteredWithFunServer();

    HTTPClient http;
    String url = String(FUN_FACTS_BASE_URL) + "/v1/fun/facts/mixed?count=1";
    if (!http.begin(url)) {
        return false;
    }
    addFunHeaders(http);

    int httpCode = http.GET();
    if (httpCode <= 0) {
        Serial.printf("HTTP mixed facts failed: %s\n", http.errorToString(httpCode).c_str());
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();
    payload.trim();

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.print("Mixed facts JSON error: ");
        Serial.println(error.c_str());
        return false;
    }
    if (!doc.containsKey("facts") || !doc["facts"].is<JsonArray>() || doc["facts"].as<JsonArrayConst>().size() == 0) {
        Serial.println("Mixed facts: empty or missing facts array");
        return false;
    }
    JsonObjectConst first = doc["facts"][0].as<JsonObjectConst>();
    return funSlideFromJson(first, out);
}

bool fetchSpecialSlide(FunSlide& out) {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    ensureRegisteredWithFunServer();
    if (ColdStartBle::getStoredDeviceId().length() == 0) {
        return false;
    }

    HTTPClient http;
    String url = String(FUN_FACTS_BASE_URL) + "/v1/fun/special";
    if (!http.begin(url)) {
        return false;
    }
    addFunHeaders(http);

    int httpCode = http.GET();
    if (httpCode == 204) {
        http.end();
        return false;
    }
    if (httpCode <= 0) {
        Serial.printf("[FunFetch] Special slide HTTP failed: %s\n", http.errorToString(httpCode).c_str());
        http.end();
        return false;
    }
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[FunFetch] Special slide HTTP status=%d\n", httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();
    payload.trim();

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.print("[FunFetch] Special slide JSON error: ");
        Serial.println(error.c_str());
        return false;
    }
    if (!doc.is<JsonObject>()) {
        Serial.println("[FunFetch] Special slide JSON error: not an object");
        return false;
    }
    if (!funSlideFromJson(doc.as<JsonObjectConst>(), out)) {
        return false;
    }
    // Older servers: hold for one UTC hour locally (still capped server-side when possible).
    if (out.displayHoldUntilEpoch == 0 && utcClockProbablyValid()) {
        time_t deadline = time(nullptr) + static_cast<time_t>(3600);
        out.displayHoldUntilEpoch = static_cast<uint32_t>(deadline);
    }
    if (out.displayHoldUntilEpoch != 0) {
        persistSpecialHold(out);
    }
    return true;
}

String getRoomData() {
    initI2C();

    float temperature = sht31.readTemperature();
    temperature = temperature * 9.0 / 5.0 + 32.0;
    float humidity = sht31.readHumidity();
    String result = String("Room Temp & Humidity\n");
    result += String("Temp: ") + String(temperature, 1) + "°F\n";
    result += String("Humidity: ") + String(humidity, 1) + "%";

    if (WiFi.status() == WL_CONNECTED) {
        int rssi = WiFi.RSSI();
        String strengthDesc;
        if (rssi > -50) {
            strengthDesc = "Excellent";
        } else if (rssi >= -60 && rssi <= -50) {
            strengthDesc = "Great";
        } else if (rssi >= -70 && rssi < -60) {
            strengthDesc = "Good";
        } else if (rssi >= -80 && rssi < -70) {
            strengthDesc = "Fair";
        } else if (rssi >= -90 && rssi < -80) {
            strengthDesc = "Weak";
        } else {
            strengthDesc = "Very Poor";
        }
        result += String("\nWiFi:") + String(rssi) + " dBm (" + strengthDesc + ")";
    }

    return result;
}
