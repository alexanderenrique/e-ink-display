#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <ArduinoJson.h>

class OTAManager {
public:
    OTAManager();
    void begin();
    void handle();
    bool isUpdating();
    
    // HTTPS OTA configuration
    void setVersionCheckUrl(const char* url);
    void setRootCA(const char* rootCA);
    void setPassword(const char* password);
    void setCurrentVersion(const char* version);
    
    // Check for updates and perform update if available
    bool checkForUpdate();
    bool performUpdate();

private:
    bool _initialized;
    bool _updating;
    char _versionCheckUrl[256];
    char _rootCA[4096];  // Root CA certificate
    char _password[64];
    char _currentVersion[32];
    char _firmwareUrl[256];
    
    int compareVersions(const char* version1, const char* version2);
    bool downloadFirmware(const char* url, esp_ota_handle_t ota_handle);
};

#endif // OTA_MANAGER_H
