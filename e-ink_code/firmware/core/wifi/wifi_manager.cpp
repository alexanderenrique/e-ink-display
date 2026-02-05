#include "wifi_manager.h"

WiFiManager::WiFiManager() : _initialized(false), _ssid(nullptr), _password(nullptr) {
}

bool WiFiManager::begin(const char* ssid, const char* password) {
    _ssid = ssid;
    _password = password;
    
    // Enable WiFi only when needed
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // Disable WiFi sleep for better performance during active use
    
    Serial.print("[WiFi] Attempting to connect to WiFi: ");
    Serial.println(_ssid);
    
    WiFi.begin(_ssid, _password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.print(" [");
    Serial.print(attempts * 500);
    Serial.print("ms]");
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("[WiFi] WiFi connected!");
        Serial.print("[WiFi] IP address: ");
        Serial.println(WiFi.localIP());
        
        // Get and display RSSI (signal strength)
        int rssi = WiFi.RSSI();
        Serial.print("[WiFi] Signal strength (RSSI): ");
        Serial.print(rssi);
        Serial.print(" dBm");
        
        // Add human-readable strength description
        if (rssi > -50) {
            Serial.println(" (Excellent)");
        } else if (rssi >= -60 && rssi <= -50) {
            Serial.println(" (Great)");
        } else if (rssi >= -70 && rssi < -60) {
            Serial.println(" (Good)");
        } else if (rssi >= -80 && rssi < -70) {
            Serial.println(" (Fair)");
        } else if (rssi >= -90 && rssi < -80) {
            Serial.println(" (Weak)");
        } else {
            Serial.println(" (Very Poor)");
        }
        
        _initialized = true;
        return true;
    } else {
        Serial.println();
        Serial.print("[WiFi] WiFi connection failed! Status: ");
        Serial.println(getWifiStatusString(WiFi.status()));
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
