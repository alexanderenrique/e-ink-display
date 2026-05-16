#include "fetch.h"
#include "config.h"
#include "../../core/bluetooth/cold_start_ble.h"
#include <Adafruit_SHT31.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <time.h>
#include <cstring>

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

static void logFunHttpBody(const char* label, int httpCode, const String& payload) {
    if (httpCode <= 0) {
        Serial.printf("[FunFetch] %s: transport error (%s)\n", label,
                      HTTPClient::errorToString(httpCode).c_str());
        return;
    }
    Serial.printf("[FunFetch] %s: HTTP %d, body %u bytes\n", label, httpCode,
                  static_cast<unsigned>(payload.length()));
    if (payload.length() == 0) {
        return;
    }
    const size_t preview = payload.length() > 220 ? 220 : payload.length();
    Serial.printf("[FunFetch] %s body: %.*s%s\n", label, static_cast<int>(preview),
                  payload.c_str(), payload.length() > preview ? "..." : "");
}

/** Begin HTTP(S) for fun API. When @p tls is non-null, it must outlive @p http until http.end(). */
static bool beginFunHttp(HTTPClient& http, const String& url, WiFiClientSecure* tls) {
    if (url.startsWith("https://")) {
        if (tls == nullptr) {
            Serial.println("[FunFetch] TLS: https URL but tls client is null");
            return false;
        }
        const char* ca = ROOT_CA_CERT;
        if (std::strstr(ca, "YOUR_ROOT_CA_CERTIFICATE_HERE") != nullptr || std::strlen(ca) < 120) {
            Serial.println(
                "[FunFetch] TLS: ROOT_CA_CERT not set; using setInsecure() — paste your CA PEM for production");
            tls->setInsecure();
        } else {
            Serial.println("[FunFetch] TLS: verifying server with ROOT_CA_CERT");
            tls->setCACert(ca);
        }
        if (!http.begin(*tls, url)) {
            Serial.printf("[FunFetch] http.begin(https) failed for %s\n", url.c_str());
            return false;
        }
        return true;
    }
    if (!http.begin(url)) {
        Serial.printf("[FunFetch] http.begin(http) failed for %s\n", url.c_str());
        return false;
    }
    return true;
}

/** Obtain server-assigned device_id when NVS slot is empty; phone-mint id skips POST. */
static bool ensureRegisteredWithFunServer() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    String deviceId = ColdStartBle::getStoredDeviceId();
    if (deviceId.length() > 0) {
        Serial.printf("[FunFetch] register: skip (device_id in NVS, %u chars)\n",
                      static_cast<unsigned>(deviceId.length()));
        return true;
    }

    String url = String(FUN_FACTS_BASE_URL) + "/v1/devices/register";
    Serial.printf("[FunFetch] register: POST %s\n", url.c_str());
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
    WiFiClientSecure tls;
    if (!beginFunHttp(http, url, &tls)) {
        Serial.println("[FunFetch] register: http.begin failed");
        return false;
    }
    if (strlen(FUN_FACTS_API_KEY) > 0) {
        http.addHeader("X-Fun-Key", FUN_FACTS_API_KEY);
    }
    http.addHeader("Content-Type", "application/json");

    String bodyStr;
    serializeJson(bodyDoc, bodyStr);

    Serial.printf("[FunFetch] register: friendly_name=%s mac=%s\n", friendly.c_str(),
                  WiFi.macAddress().c_str());

    int httpCode = http.POST(bodyStr);
    String payload = http.getString();
    http.end();
    logFunHttpBody("register", httpCode, payload);

    if (httpCode <= 0) {
        return false;
    }
    if (httpCode != HTTP_CODE_OK) {
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
    Serial.printf("[FunFetch] Registered with fun server (device_id=%s)\n", newId);
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

    if (!ensureRegisteredWithFunServer()) {
        Serial.println("[FunFetch] screen: device registration failed");
        return false;
    }

    HTTPClient http;
    WiFiClientSecure tls;
    String url = String(FUN_FACTS_BASE_URL) + "/v1/fun/screen?m=" + String(mode);
    Serial.printf("[FunFetch] screen: GET %s\n", url.c_str());
    if (!beginFunHttp(http, url, &tls)) {
        return false;
    }
    addFunHeaders(http);
    String did = ColdStartBle::getStoredDeviceId();
    Serial.printf("[FunFetch] screen: X-Device-Id %s\n",
                  did.length() > 0 ? did.c_str() : "(none)");

    int httpCode = http.GET();
    bool ok = false;
    if (httpCode > 0) {
        String payload = http.getString();
        payload.trim();
        logFunHttpBody("screen", httpCode, payload);
        if (httpCode == HTTP_CODE_OK) {
            DynamicJsonDocument doc(4096);
            DeserializationError error = deserializeJson(doc, payload);
            if (!error && doc.is<JsonObject>()) {
                ok = funSlideFromJson(doc.as<JsonObjectConst>(), out);
                if (!ok) {
                    Serial.println("[FunFetch] screen: JSON ok but missing or empty text field");
                } else {
                    Serial.printf("[FunFetch] screen: ok, text %u chars, layout=%s\n",
                                  static_cast<unsigned>(out.text.length()), out.layout.c_str());
                }
            } else if (error) {
                Serial.print("[FunFetch] screen JSON error: ");
                Serial.println(error.c_str());
            }
        }
    } else {
        logFunHttpBody("screen", httpCode, String());
    }
    http.end();
    return ok;
}

bool fetchMixedFunSlide(FunSlide& out) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected!");
        return false;
    }

    if (!ensureRegisteredWithFunServer()) {
        Serial.println("[FunFetch] mixed: device registration failed");
        return false;
    }

    HTTPClient http;
    WiFiClientSecure tls;
    String url = String(FUN_FACTS_BASE_URL) + "/v1/fun/facts/mixed?count=1";
    Serial.printf("[FunFetch] mixed: GET %s\n", url.c_str());
    if (!beginFunHttp(http, url, &tls)) {
        return false;
    }
    addFunHeaders(http);

    int httpCode = http.GET();
    String payload = http.getString();
    http.end();
    payload.trim();
    logFunHttpBody("mixed", httpCode, payload);

    if (httpCode <= 0) {
        return false;
    }
    if (httpCode != HTTP_CODE_OK) {
        return false;
    }

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.print("[FunFetch] mixed JSON error: ");
        Serial.println(error.c_str());
        return false;
    }
    if (!doc.containsKey("facts") || !doc["facts"].is<JsonArray>() || doc["facts"].as<JsonArrayConst>().size() == 0) {
        Serial.println("[FunFetch] mixed: empty or missing facts array");
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
    WiFiClientSecure tls;
    String url = String(FUN_FACTS_BASE_URL) + "/v1/fun/special";
    if (!beginFunHttp(http, url, &tls)) {
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
