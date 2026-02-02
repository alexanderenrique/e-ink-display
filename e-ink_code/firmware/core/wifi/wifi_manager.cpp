#include "wifi_manager.h"

WiFiManager::WiFiManager() : _initialized(false), _ssid(nullptr), _password(nullptr) {
}

bool WiFiManager::begin(const char* ssid, const char* password) {
    _ssid = ssid;
    _password = password;
    
    // Enable WiFi only when needed
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // Disable WiFi sleep for better performance during active use
    
    Serial.print("Connecting to WiFi: ");
    Serial.println(_ssid);
    
    WiFi.begin(_ssid, _password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("WiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        _initialized = true;
        return true;
    } else {
        Serial.println();
        Serial.println("WiFi connection failed!");
        _initialized = false;
        return false;
    }
}

void WiFiManager::disconnect() {
    // Explicitly disconnect and turn off WiFi
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi disabled");
    _initialized = false;
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

int WiFiManager::getRSSI() {
    if (!isConnected()) {
        Serial.println("[WiFi] Not connected, cannot get RSSI");
        return -100; // Return a very poor value to indicate no connection
    }
    
    int rssi = WiFi.RSSI();
    Serial.print("[WiFi] RSSI: ");
    Serial.println(rssi);
    return rssi;
}

IPAddress WiFiManager::getLocalIP() {
    if (isConnected()) {
        return WiFi.localIP();
    }
    return IPAddress(0, 0, 0, 0);
}

String WiFiManager::getStatusString() {
    return getWifiStatusString(WiFi.status());
}

String WiFiManager::getWifiStatusString(wl_status_t status) {
    switch(status) {
        case WL_NO_SHIELD: return "NO_SHIELD";
        case WL_IDLE_STATUS: return "IDLE_STATUS";
        case WL_NO_SSID_AVAIL: return "NO_SSID_AVAIL";
        case WL_SCAN_COMPLETED: return "SCAN_COMPLETED";
        case WL_CONNECTED: return "CONNECTED";
        case WL_CONNECT_FAILED: return "CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "CONNECTION_LOST";
        case WL_DISCONNECTED: return "DISCONNECTED";
        default: return "UNKNOWN";
    }
}
