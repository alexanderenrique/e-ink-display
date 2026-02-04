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

#define PENDING_CONFIG_MAX 2048

// Pending config: filled by BLE callback, processed in main loop (avoids crash from heavy work in BLE task)
static char s_pendingConfigBuffer[PENDING_CONFIG_MAX];
static size_t s_pendingConfigLength = 0;
static bool s_pendingConfig = false;

/** Callback that sets a flag when a central connects.
 *  Keep this minimal (no Serial, no allocation): runs in BLE task context; heavy work can crash.
 */
class ColdStartBleServerCallbacks : public NimBLEServerCallbacks {
public:
    explicit ColdStartBleServerCallbacks(bool* connectedFlag) : _connectedFlag(connectedFlag) {}
    void onConnect(NimBLEServer* /*pServer*/) override {
        *_connectedFlag = true;
    }
    void onDisconnect(NimBLEServer* /*pServer*/) override {
        *_connectedFlag = false;
    }

private:
    bool* _connectedFlag;
};

/** Callback that handles data written to the TX characteristic.
 *  Only copies payload to a buffer and sets a flag. Heavy work (Preferences, restart)
 *  is done in ColdStartBle::loop() to avoid crashing (BLE callbacks run in BLE task).
 */
class ColdStartBleTxCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        std::string value = pCharacteristic->getValue();
        size_t length = value.length();
        if (length == 0 || s_pendingConfig) return;
        size_t copyLen = (length < PENDING_CONFIG_MAX) ? length : (PENDING_CONFIG_MAX - 1);
        memcpy(s_pendingConfigBuffer, value.data(), copyLen);
        s_pendingConfigBuffer[copyLen] = '\0';
        s_pendingConfigLength = copyLen;
        s_pendingConfig = true;
    }
};

/** Process config received via BLE. Called from main loop (not from BLE callback). */
static void processPendingConfig() {
    String jsonString(s_pendingConfigBuffer);
    s_pendingConfigLength = 0;
    if (jsonString.length() == 0 || !jsonString.startsWith("{") || !jsonString.endsWith("}")) {
        Serial.println("[ColdStartBle] Pending config invalid or not JSON, ignoring");
        return;
    }
    Serial.println("[ColdStartBle] Processing received config...");
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) {
        Serial.print("[ColdStartBle] JSON parse error: ");
        Serial.println(error.c_str());
        return;
    }
    Preferences preferences;
    preferences.begin("config", false);
    const char* wifiSSID = nullptr;
    if (doc.containsKey("wifiSSID")) wifiSSID = doc["wifiSSID"];
    else if (doc.containsKey("wifiSsid")) wifiSSID = doc["wifiSsid"];
    if (wifiSSID != nullptr) preferences.putString("wifiSSID", wifiSSID);
    if (doc.containsKey("wifiPassword")) preferences.putString("wifiPassword", doc["wifiPassword"].as<const char*>());
    if (doc.containsKey("mode")) preferences.putString("mode", doc["mode"].as<const char*>());
    if (doc.containsKey("refreshInterval")) preferences.putUInt("refreshInterval", doc["refreshInterval"].as<uint32_t>());
    if (doc.containsKey("timestamp")) preferences.putULong64("timestamp", doc["timestamp"].as<uint64_t>());
    if (doc.containsKey("apis") && doc["apis"].is<JsonObject>()) {
        String apisJson;
        serializeJson(doc["apis"], apisJson);
        preferences.putString("apis", apisJson);
    }
    preferences.putString("configJson", jsonString);
    preferences.putBool("skipBLE", true);
    preferences.end();
    Serial.println("[ColdStartBle] Configuration saved, restarting...");
    Serial.flush();
    delay(1000);
    ESP.restart();
}

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
    Serial.println("[ColdStartBle] BLE enabled for 3 minutes or until connected (cold start)");
}

void ColdStartBle::loop() {
    // Config received in BLE callback: process in main loop to avoid crash (no heavy work in BLE task)
    if (s_pendingConfig) {
        s_pendingConfig = false;
        NimBLEDevice::deinit(true);
        _active = false;
        processPendingConfig();
        return;
    }

    if (!_active) {
        return;
    }

    uint32_t elapsed = (uint32_t)(millis() - _startMillis);
    bool timeout = (elapsed >= (COLD_START_BLE_WINDOW_SECONDS * 1000u));

    // Log connection state changes from main loop (not from BLE callback, to avoid crash)
    static bool lastConnected = false;
    if (_connected && !lastConnected) {
        Serial.println("[ColdStartBle] Client connected");
    }
    if (!_connected && lastConnected) {
        Serial.println("[ColdStartBle] Client disconnected");
    }
    lastConnected = _connected;
    
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

    // Only disable BLE when the window has timed out and no one is connected.
    // While a client is connected we stay up so they can discover services and send config.
    if (timeout && !_connected) {
        NimBLEDevice::deinit(true);
        _active = false;
        if (lastConnected) {
            Serial.println("[ColdStartBle] BLE disabled (client disconnected)");
        } else {
            Serial.println("[ColdStartBle] BLE disabled after timeout");
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
