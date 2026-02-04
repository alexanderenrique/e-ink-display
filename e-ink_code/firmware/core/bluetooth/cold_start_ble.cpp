#include "cold_start_ble.h"
#include "NimBLEDevice.h"
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// Compile-time check for BLE support
#ifndef CONFIG_BT_ENABLED
#warning "CONFIG_BT_ENABLED not defined - BLE may not be enabled in sdkconfig"
#endif

#ifndef CONFIG_BT_NIMBLE_ENABLED
#warning "CONFIG_BT_NIMBLE_ENABLED not defined - NimBLE may not be enabled"
#endif

#ifndef COLD_START_BLE_DEVICE_NAME
#define COLD_START_BLE_DEVICE_NAME "E-Ink Display"
#endif

// Standard Device Information Service UUID (0x180A) - helps with service discovery
#define DEVICE_INFO_SERVICE_UUID "180A"

// Custom service UUID matching web client expectations
#define COLD_START_SERVICE_UUID "0000ff00-0000-1000-8000-00805f9b34fb"

// Standard Model Number String characteristic UUID (0x2A24) for Device Information Service
#define MODEL_NUMBER_CHAR_UUID "2A24"

// TX and RX characteristic UUIDs matching web client expectations
#define COLD_START_TX_CHAR_UUID "0000ff01-0000-1000-8000-00805f9b34fb"
#define COLD_START_RX_CHAR_UUID "0000ff02-0000-1000-8000-00805f9b34fb"

/** Callback that sets a flag when a central connects. */
class ColdStartBleServerCallbacks : public NimBLEServerCallbacks {
public:
    explicit ColdStartBleServerCallbacks(bool* connectedFlag) : _connectedFlag(connectedFlag) {}
    void onConnect(NimBLEServer* /*pServer*/) override {
        *_connectedFlag = true;
        Serial.println("[ColdStartBle] Client connected");
    }
    void onDisconnect(NimBLEServer* /*pServer*/) override {
        Serial.println("[ColdStartBle] Client disconnected");
    }

private:
    bool* _connectedFlag;
};

/** Callback that handles data written to the TX characteristic. */
class ColdStartBleTxCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        Serial.println("[ColdStartBle] ========================================");
        Serial.println("[ColdStartBle] DATA RECEIVED VIA BLE");
        Serial.println("[ColdStartBle] ========================================");
        
        // Get the value that was written
        std::string value = pCharacteristic->getValue();
        size_t length = value.length();
        
        Serial.print("[ColdStartBle] Characteristic UUID: ");
        Serial.println(pCharacteristic->getUUID().toString().c_str());
        Serial.print("[ColdStartBle] Data length: ");
        Serial.print(length);
        Serial.println(" bytes");
        
        if (length == 0) {
            Serial.println("[ColdStartBle] WARNING: Received empty data!");
            return;
        }
        
        // Print raw bytes (decimal)
        Serial.print("[ColdStartBle] Raw bytes (decimal, first 100): ");
        size_t previewLen = (length > 100) ? 100 : length;
        for (size_t i = 0; i < previewLen; i++) {
            if (i > 0) Serial.print(" ");
            Serial.print((uint8_t)value[i]);
        }
        if (length > 100) {
            Serial.print(" ... (truncated, showing first 100 of ");
            Serial.print(length);
            Serial.println(" bytes)");
        } else {
            Serial.println();
        }
        
        // Print raw bytes (hex)
        Serial.print("[ColdStartBle] Raw bytes (hex, first 100): ");
        for (size_t i = 0; i < previewLen; i++) {
            if (i > 0) Serial.print(" ");
            uint8_t byte = (uint8_t)value[i];
            if (byte < 0x10) Serial.print("0");
            Serial.print(byte, HEX);
        }
        if (length > 100) {
            Serial.print(" ... (truncated)");
        }
        Serial.println();
        
        // Try to interpret as string/JSON
        Serial.println("[ColdStartBle] Attempting to decode as string...");
        String jsonString = "";
        for (size_t i = 0; i < length; i++) {
            char c = value[i];
            if (c >= 32 && c <= 126) {  // Printable ASCII
                jsonString += c;
            } else {
                jsonString += "?";
            }
        }
        
        Serial.print("[ColdStartBle] Decoded string length: ");
        Serial.println(jsonString.length());
        Serial.print("[ColdStartBle] Decoded string (first 200 chars): ");
        if (jsonString.length() > 200) {
            Serial.println(jsonString.substring(0, 200));
            Serial.print("[ColdStartBle] ... (truncated, total length: ");
            Serial.print(jsonString.length());
            Serial.println(" chars)");
        } else {
            Serial.println(jsonString);
        }
        
        // Check if it looks like JSON
        if (jsonString.startsWith("{") && jsonString.endsWith("}")) {
            Serial.println("[ColdStartBle] ✓ Detected JSON format");
            
            // Parse JSON configuration
            Serial.println("[ColdStartBle] Parsing JSON configuration...");
            DynamicJsonDocument doc(2048);
            DeserializationError error = deserializeJson(doc, jsonString);
            
            if (error) {
                Serial.print("[ColdStartBle] JSON parse error: ");
                Serial.println(error.c_str());
                Serial.println("[ColdStartBle] ========================================");
                Serial.println("[ColdStartBle] END OF DATA RECEIVED (PARSE FAILED)");
                Serial.println("[ColdStartBle] ========================================");
                return;
            }
            
            Serial.println("[ColdStartBle] ✓ JSON parsed successfully");
            
            // Store configuration in Preferences
            Preferences preferences;
            preferences.begin("config", false); // false = read/write mode
            
            // Store WiFi credentials if provided
            // Check for both "wifiSSID" and "wifiSsid" variants (JSON key case sensitivity)
            const char* wifiSSID = nullptr;
            if (doc.containsKey("wifiSSID")) {
                wifiSSID = doc["wifiSSID"];
            } else if (doc.containsKey("wifiSsid")) {
                wifiSSID = doc["wifiSsid"];
            }
            
            if (wifiSSID != nullptr) {
                preferences.putString("wifiSSID", wifiSSID);
                Serial.print("[ColdStartBle] Stored WiFi SSID: ");
                Serial.println(wifiSSID);
            } else {
                Serial.println("[ColdStartBle] ⚠ No wifiSSID or wifiSsid in config");
            }
            
            if (doc.containsKey("wifiPassword")) {
                const char* wifiPassword = doc["wifiPassword"];
                preferences.putString("wifiPassword", wifiPassword);
                Serial.print("[ColdStartBle] Stored WiFi Password: ");
                Serial.println("*** (hidden)");
            } else {
                Serial.println("[ColdStartBle] ⚠ No wifiPassword in config");
            }
            
            // Store app mode
            if (doc.containsKey("mode")) {
                const char* mode = doc["mode"];
                preferences.putString("mode", mode);
                Serial.print("[ColdStartBle] Stored mode: ");
                Serial.println(mode);
            }
            
            // Store refresh interval (in minutes)
            if (doc.containsKey("refreshInterval")) {
                uint32_t refreshInterval = doc["refreshInterval"];
                preferences.putUInt("refreshInterval", refreshInterval);
                Serial.print("[ColdStartBle] Stored refreshInterval: ");
                Serial.print(refreshInterval);
                Serial.println(" minutes");
            }
            
            // Store timestamp
            if (doc.containsKey("timestamp")) {
                uint64_t timestamp = doc["timestamp"];
                preferences.putULong64("timestamp", timestamp);
                Serial.print("[ColdStartBle] Stored timestamp: ");
                Serial.println(timestamp);
            }
            
            // Store APIs config as JSON string (nested object)
            if (doc.containsKey("apis") && doc["apis"].is<JsonObject>()) {
                String apisJson;
                serializeJson(doc["apis"], apisJson);
                preferences.putString("apis", apisJson);
                Serial.print("[ColdStartBle] Stored APIs config: ");
                Serial.println(apisJson);
            }
            
            // Store full config JSON for app manager
            preferences.putString("configJson", jsonString);
            Serial.println("[ColdStartBle] Stored full config JSON");
            
            // Set flag to skip BLE on next boot (after restart)
            // Note: ESP32 Preferences key max length is 15 characters
            bool flagSet = preferences.putBool("skipBLE", true);
            Serial.print("[ColdStartBle] Set skipBLE flag: ");
            Serial.println(flagSet ? "SUCCESS" : "FAILED");
            
            // Verify the flag was set
            bool flagVerify = preferences.getBool("skipBLE", false);
            Serial.print("[ColdStartBle] Verified skipBLE flag: ");
            Serial.println(flagVerify ? "TRUE" : "FALSE");
            
            preferences.end();
            Serial.println("[ColdStartBle] ✓ Configuration saved to Preferences");
            
            // Flush serial and give Preferences time to commit
            Serial.flush();
            delay(1000);  // Give Preferences time to commit to NVS
            
            // Restart to apply the new configuration
            Serial.println("[ColdStartBle] Restarting device to apply configuration...");
            Serial.flush();
            delay(500);
            ESP.restart();
            
        } else {
            Serial.println("[ColdStartBle] ⚠ Does not appear to be JSON");
        }
        
        // Print byte-by-byte analysis for first 20 bytes
        Serial.println("[ColdStartBle] Byte-by-byte analysis (first 20):");
        size_t analysisLen = (length > 20) ? 20 : length;
        for (size_t i = 0; i < analysisLen; i++) {
            uint8_t byte = (uint8_t)value[i];
            Serial.print("[ColdStartBle]   [");
            Serial.print(i);
            Serial.print("] ");
            Serial.print(byte);
            Serial.print(" (0x");
            if (byte < 0x10) Serial.print("0");
            Serial.print(byte, HEX);
            Serial.print(") = '");
            if (byte >= 32 && byte <= 126) {
                Serial.print((char)byte);
            } else {
                Serial.print("?");
            }
            Serial.println("'");
        }
        
        Serial.println("[ColdStartBle] ========================================");
        Serial.println("[ColdStartBle] END OF DATA RECEIVED");
        Serial.println("[ColdStartBle] ========================================");
    }
};

ColdStartBle::ColdStartBle() : _active(false), _startMillis(0), _connected(false) {}

void ColdStartBle::begin(esp_sleep_wakeup_cause_t wakeup_cause) {
    if (wakeup_cause != ESP_SLEEP_WAKEUP_UNDEFINED) {
        Serial.print("[ColdStartBle] Skipping BLE mode - wakeup cause: ");
        Serial.println(wakeup_cause);
        return;
    }

    // Check if we should skip BLE (e.g., after config-triggered restart)
    if (shouldSkipBle()) {
        Serial.println("[ColdStartBle] ✓ Skipping BLE mode - config was just received, applying changes");
        return;
    }

    Serial.println("[ColdStartBle] Entering BLE mode (cold start detected)");
    
    // Disable WiFi to ensure BLE has exclusive access to the radio (ESP32C3 requirement)
    WiFi.mode(WIFI_OFF);
    WiFi.disconnect(true);
    delay(100);  // Give WiFi time to fully shut down
    Serial.println("[ColdStartBle] WiFi disabled for BLE");
    
    Serial.print("[ColdStartBle] Initializing BLE device: ");
    Serial.println(COLD_START_BLE_DEVICE_NAME);
    
    // Initialize BLE device with name
    NimBLEDevice::init(COLD_START_BLE_DEVICE_NAME);
    Serial.println("[ColdStartBle] BLE device initialized successfully");

    // Print BLE MAC address for debugging
    Serial.print("[ColdStartBle] BLE MAC Address: ");
    Serial.println(NimBLEDevice::getAddress().toString().c_str());

    // Set power level (optional, but can help with range)
    // ESP_PWR_LVL_P9 = maximum power (19.5 dBm)
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    Serial.println("[ColdStartBle] BLE power set to maximum (19.5 dBm)");

    NimBLEServer* pServer = NimBLEDevice::createServer();
    if (!pServer) {
        NimBLEDevice::deinit(true);
        Serial.println("[ColdStartBle] ERROR: createServer failed");
        return;
    }
    Serial.println("[ColdStartBle] BLE server created");

    pServer->setCallbacks(new ColdStartBleServerCallbacks(&_connected));

    // Create standard Device Information Service (0x180A) for better discoverability
    NimBLEService* pDeviceInfoService = pServer->createService(DEVICE_INFO_SERVICE_UUID);
    if (!pDeviceInfoService) {
        NimBLEDevice::deinit(true);
        Serial.println("[ColdStartBle] ERROR: Failed to create Device Information Service");
        return;
    }
    
    // Add Model Number characteristic to Device Information Service
    NimBLECharacteristic* pModelChar = pDeviceInfoService->createCharacteristic(
        MODEL_NUMBER_CHAR_UUID,
        NIMBLE_PROPERTY::READ
    );
    if (pModelChar) {
        pModelChar->setValue("E-Ink Display");
        Serial.println("[ColdStartBle] Device Information Service characteristic created");
    }
    
    if (!pDeviceInfoService->start()) {
        NimBLEDevice::deinit(true);
        Serial.println("[ColdStartBle] ERROR: Failed to start Device Information Service");
        return;
    }
    Serial.println("[ColdStartBle] Device Information Service (0x180A) created and started");

    // Create custom service (characteristics must be created BEFORE starting the service)
    NimBLEService* pService = pServer->createService(COLD_START_SERVICE_UUID);
    if (!pService) {
        NimBLEDevice::deinit(true);
        Serial.println("[ColdStartBle] ERROR: Failed to create BLE service");
        return;
    }
    Serial.println("[ColdStartBle] Custom BLE service created");

    // Create TX characteristic (for receiving data from client)
    NimBLECharacteristic* pTxChar = pService->createCharacteristic(
        COLD_START_TX_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    if (!pTxChar) {
        NimBLEDevice::deinit(true);
        Serial.println("[ColdStartBle] ERROR: Failed to create TX characteristic");
        return;
    }
    Serial.println("[ColdStartBle] TX characteristic created");
    Serial.print("  - TX Characteristic UUID: ");
    Serial.println(COLD_START_TX_CHAR_UUID);
    
    // Register callback to handle incoming data
    pTxChar->setCallbacks(new ColdStartBleTxCallbacks());
    Serial.println("[ColdStartBle] TX characteristic callback registered - ready to receive data");

    // Create RX characteristic (for sending data to client)
    NimBLECharacteristic* pRxChar = pService->createCharacteristic(
        COLD_START_RX_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    if (!pRxChar) {
        NimBLEDevice::deinit(true);
        Serial.println("[ColdStartBle] ERROR: Failed to create RX characteristic");
        return;
    }
    Serial.println("[ColdStartBle] RX characteristic created");
    Serial.print("  - RX Characteristic UUID: ");
    Serial.println(COLD_START_RX_CHAR_UUID);

    // NOW start the service (after all characteristics are created)
    if (!pService->start()) {
        NimBLEDevice::deinit(true);
        Serial.println("[ColdStartBle] ERROR: Failed to start BLE service");
        return;
    }
    Serial.println("[ColdStartBle] Custom BLE service started");

    // CRITICAL: Start the GATT server after all services are created and started
    // This makes the services discoverable to clients
    pServer->start();
    Serial.println("[ColdStartBle] GATT server started - services are now discoverable");

    // Configure advertising: addServiceUUID then start (order: service->start(); server->start(); adv->addServiceUUID(); adv->start();)
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    if (!pAdvertising) {
        NimBLEDevice::deinit(true);
        Serial.println("[ColdStartBle] ERROR: Failed to get advertising object");
        return;
    }
    
    // CRITICAL: Add all service UUIDs to advertising - this makes them visible in scan results
    // Add both the custom service and Device Information Service so they're discoverable
    pAdvertising->addServiceUUID(pService->getUUID());
    pAdvertising->addServiceUUID(pDeviceInfoService->getUUID());
    
    // Enable scan response for better discoverability
    pAdvertising->setScanResponse(true);
    
    // Add TX power to advertising data (uses power level set via NimBLEDevice::setPower)
    pAdvertising->addTxPower();
    
    Serial.println("[ColdStartBle] Advertising configured:");
    Serial.print("  - Device name: ");
    Serial.println(COLD_START_BLE_DEVICE_NAME);
    Serial.print("  - Custom Service UUID: ");
    Serial.println(COLD_START_SERVICE_UUID);
    Serial.print("  - Device Info Service UUID: ");
    Serial.println(DEVICE_INFO_SERVICE_UUID);
    Serial.println("  - TX power: added to advertising data");
    
    // Small delay to ensure BLE stack is ready
    delay(200);

    // Start advertising (after service started and adv data set)
    if (!pAdvertising->start()) {
        NimBLEDevice::deinit(true);
        Serial.println("[ColdStartBle] ERROR: Failed to start advertising");
        return;
    }
    
    Serial.println("[ColdStartBle] ✓ BLE advertising started successfully");
    Serial.println("[ColdStartBle] Device should be discoverable as 'E-Ink Display'");

    _active = true;
    _startMillis = millis();
    _connected = false;
    Serial.println("[ColdStartBle] BLE enabled for 60s or until connected (cold start)");
}

void ColdStartBle::loop() {
    if (!_active) {
        return;
    }

    uint32_t elapsed = (uint32_t)(millis() - _startMillis);
    bool timeout = (elapsed >= (COLD_START_BLE_WINDOW_SECONDS * 1000u));
    
    // Print status every 5 seconds with more details
    static uint32_t lastStatusPrint = 0;
    if (elapsed - lastStatusPrint >= 5000) {
        uint32_t remaining = (COLD_START_BLE_WINDOW_SECONDS * 1000u) - elapsed;
        Serial.print("[ColdStartBle] Still advertising (active=");
        Serial.print(_active);
        Serial.print(", connected=");
        Serial.print(_connected);
        Serial.print("), ");
        Serial.print(remaining / 1000);
        Serial.println(" seconds remaining");
        
        // Verify advertising is still active
        NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
        if (pAdv) {
            Serial.println("[ColdStartBle] Advertising object exists");
        } else {
            Serial.println("[ColdStartBle] WARNING: Advertising object is null!");
        }
        lastStatusPrint = elapsed;
    }

    if (_connected || timeout) {
        NimBLEDevice::deinit(true);
        _active = false;
        if (_connected) {
            Serial.println("[ColdStartBle] BLE disabled after connection");
        } else {
            Serial.println("[ColdStartBle] BLE disabled after 60s window");
        }
    }
}

String ColdStartBle::getStoredWiFiSSID() {
    Preferences preferences;
    preferences.begin("config", true); // true = read-only mode
    String ssid = preferences.getString("wifiSSID", "");
    preferences.end();
    return ssid;
}

String ColdStartBle::getStoredWiFiPassword() {
    Preferences preferences;
    preferences.begin("config", true); // true = read-only mode
    String password = preferences.getString("wifiPassword", "");
    preferences.end();
    return password;
}

String ColdStartBle::getStoredConfigJson() {
    Preferences preferences;
    preferences.begin("config", true); // true = read-only mode
    String configJson = preferences.getString("configJson", "");
    preferences.end();
    return configJson;
}

bool ColdStartBle::hasStoredConfig() {
    Preferences preferences;
    preferences.begin("config", true); // true = read-only mode
    bool hasConfig = preferences.isKey("configJson");
    preferences.end();
    return hasConfig;
}

bool ColdStartBle::shouldSkipBle() {
    Preferences preferences;
    if (!preferences.begin("config", false)) { // false = read/write mode
        Serial.println("[ColdStartBle] ERROR: Failed to open Preferences for skipBle check");
        return false;
    }
    
    bool skipBle = preferences.getBool("skipBLE", false);
    Serial.print("[ColdStartBle] Checking skipBLE flag: ");
    Serial.println(skipBle ? "TRUE (will skip BLE)" : "FALSE (will enable BLE)");
    
    if (skipBle) {
        // Clear the flag after checking
        preferences.remove("skipBLE");
        Serial.println("[ColdStartBle] ✓ Found skipBLE flag, cleared it");
    }
    preferences.end();
    return skipBle;
}
