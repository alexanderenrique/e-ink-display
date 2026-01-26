#include "ota_manager.h"

OTAManager::OTAManager() : _initialized(false), _updating(false) {
    _versionCheckUrl[0] = '\0';
    _rootCA[0] = '\0';
    _password[0] = '\0';
    _firmwareUrl[0] = '\0';
    strncpy(_currentVersion, "1.0.0", sizeof(_currentVersion) - 1);
    _currentVersion[sizeof(_currentVersion) - 1] = '\0';
}

void OTAManager::setVersionCheckUrl(const char* url) {
    if (url) {
        strncpy(_versionCheckUrl, url, sizeof(_versionCheckUrl) - 1);
        _versionCheckUrl[sizeof(_versionCheckUrl) - 1] = '\0';
    }
}

void OTAManager::setRootCA(const char* rootCA) {
    if (rootCA) {
        strncpy(_rootCA, rootCA, sizeof(_rootCA) - 1);
        _rootCA[sizeof(_rootCA) - 1] = '\0';
    }
}

void OTAManager::setPassword(const char* password) {
    if (password) {
        strncpy(_password, password, sizeof(_password) - 1);
        _password[sizeof(_password) - 1] = '\0';
    } else {
        _password[0] = '\0';
    }
}

void OTAManager::setCurrentVersion(const char* version) {
    if (version) {
        strncpy(_currentVersion, version, sizeof(_currentVersion) - 1);
        _currentVersion[sizeof(_currentVersion) - 1] = '\0';
    }
}

void OTAManager::begin() {
    _initialized = true;
    Serial.println("[OTA] HTTPS OTA Manager initialized");
}

void OTAManager::handle() {
    // HTTPS OTA doesn't need continuous handling
    // Updates are triggered manually via checkForUpdate()
}

bool OTAManager::isUpdating() {
    return _updating;
}

int OTAManager::compareVersions(const char* version1, const char* version2) {
    // Simple version comparison: "1.2.3" format
    // Returns: -1 if version1 < version2, 0 if equal, 1 if version1 > version2
    int v1[3] = {0, 0, 0};
    int v2[3] = {0, 0, 0};
    
    sscanf(version1, "%d.%d.%d", &v1[0], &v1[1], &v1[2]);
    sscanf(version2, "%d.%d.%d", &v2[0], &v2[1], &v2[2]);
    
    for (int i = 0; i < 3; i++) {
        if (v1[i] < v2[i]) return -1;
        if (v1[i] > v2[i]) return 1;
    }
    return 0;
}

bool OTAManager::checkForUpdate() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[OTA] WiFi not connected, cannot check for updates");
        return false;
    }
    
    if (strlen(_versionCheckUrl) == 0) {
        Serial.println("[OTA] Version check URL not set");
        return false;
    }
    
    if (strlen(_rootCA) == 0) {
        Serial.println("[OTA] Root CA certificate not set");
        return false;
    }
    
    WiFiClientSecure client;
    
    // Set root CA certificate for certificate validation
    client.setCACert(_rootCA);
    
    HTTPClient http;
    http.begin(client, _versionCheckUrl);
    
    // Add password as header if set
    if (strlen(_password) > 0) {
        http.addHeader("X-OTA-Password", _password);
    }
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        http.end();
        
        // Parse JSON response: {"version": "1.2.3", "url": "https://server/firmware.bin"}
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
            Serial.print("[OTA] JSON parse error: ");
            Serial.println(error.c_str());
            return false;
        }
        
        const char* serverVersion = doc["version"];
        const char* firmwareUrl = doc["url"];
        
        if (!serverVersion || !firmwareUrl) {
            Serial.println("[OTA] Invalid response format");
            return false;
        }
        
        Serial.print("[OTA] Current version: ");
        Serial.println(_currentVersion);
        Serial.print("[OTA] Server version: ");
        Serial.println(serverVersion);
        
        if (compareVersions(serverVersion, _currentVersion) > 0) {
            Serial.println("[OTA] Update available!");
            // Store firmware URL for download
            strncpy(_firmwareUrl, firmwareUrl, sizeof(_firmwareUrl) - 1);
            _firmwareUrl[sizeof(_firmwareUrl) - 1] = '\0';
            return true;
        } else {
            Serial.println("[OTA] Already on latest version");
            return false;
        }
    } else {
        Serial.printf("[OTA] Version check failed: %d\n", httpCode);
        http.end();
        return false;
    }
}

bool OTAManager::downloadFirmware(const char* url, esp_ota_handle_t ota_handle) {
    WiFiClientSecure client;
    
    // Set root CA certificate for certificate validation
    client.setCACert(_rootCA);
    
    HTTPClient http;
    http.begin(client, url);
    
    // Add password as header if set
    if (strlen(_password) > 0) {
        http.addHeader("X-OTA-Password", _password);
    }
    
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[OTA] HTTP request failed: %d\n", httpCode);
        http.end();
        return false;
    }
    
    int contentLength = http.getSize();
    Serial.printf("[OTA] Firmware size: %d bytes\n", contentLength);
    
    // Read and write firmware in chunks
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[1024];
    int totalBytes = 0;
    int bytesRead = 0;
    
    while (http.connected() && (bytesRead = stream->available())) {
        if (bytesRead > 1024) bytesRead = 1024;
        
        int len = stream->readBytes(buffer, bytesRead);
        if (len > 0) {
            esp_err_t err = esp_ota_write(ota_handle, buffer, len);
            if (err != ESP_OK) {
                Serial.printf("[OTA] Write failed: %s\n", esp_err_to_name(err));
                http.end();
                return false;
            }
            totalBytes += len;
            
            // Print progress
            if (contentLength > 0) {
                int progress = (totalBytes * 100) / contentLength;
                Serial.printf("[OTA] Progress: %d%% (%d/%d bytes)\r", progress, totalBytes, contentLength);
            } else {
                Serial.printf("[OTA] Downloaded: %d bytes\r", totalBytes);
            }
        }
    }
    
    http.end();
    Serial.println();
    
    return true;
}

bool OTAManager::performUpdate() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[OTA] WiFi not connected");
        return false;
    }
    
    if (strlen(_firmwareUrl) == 0) {
        Serial.println("[OTA] Firmware URL not set");
        return false;
    }
    
    if (strlen(_rootCA) == 0) {
        Serial.println("[OTA] Root CA certificate not set");
        return false;
    }
    
    _updating = true;
    Serial.println("[OTA] Starting HTTPS firmware update...");
    Serial.print("[OTA] Downloading from: ");
    Serial.println(_firmwareUrl);
    
    // Get the next OTA partition
    const esp_partition_t* ota_partition = esp_ota_get_next_update_partition(NULL);
    if (!ota_partition) {
        Serial.println("[OTA] No OTA partition found");
        _updating = false;
        return false;
    }
    
    Serial.print("[OTA] Writing to partition: ");
    Serial.println(ota_partition->label);
    
    esp_ota_handle_t ota_handle = 0;
    
    // Initialize OTA
    esp_err_t err = esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        Serial.printf("[OTA] esp_ota_begin failed: %s\n", esp_err_to_name(err));
        _updating = false;
        return false;
    }
    
    // Download firmware via HTTPS
    if (!downloadFirmware(_firmwareUrl, ota_handle)) {
        Serial.println("[OTA] Firmware download failed");
        esp_ota_abort(ota_handle);
        _updating = false;
        return false;
    }
    
    // Finalize OTA
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        Serial.printf("[OTA] esp_ota_end failed: %s\n", esp_err_to_name(err));
        _updating = false;
        return false;
    }
    
    // Set boot partition
    err = esp_ota_set_boot_partition(ota_partition);
    if (err != ESP_OK) {
        Serial.printf("[OTA] esp_ota_set_boot_partition failed: %s\n", esp_err_to_name(err));
        _updating = false;
        return false;
    }
    
    Serial.println("[OTA] Update successful! Rebooting...");
    _updating = false;
    delay(1000);
    ESP.restart();
    
    return true;
}
