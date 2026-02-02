#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

class WiFiManager {
public:
    WiFiManager();
    bool begin(const char* ssid, const char* password);
    void disconnect();
    bool isConnected();
    int getRSSI();
    String getStatusString();
    IPAddress getLocalIP();

private:
    bool _initialized;
    const char* _ssid;
    const char* _password;
    
    String getWifiStatusString(wl_status_t status);
};

#endif // WIFI_MANAGER_H
