#include "cold_start_ble.h"
#include "NimBLEDevice.h"
#include <WiFi.h>

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

// Custom 128-bit service UUID (advertised so scanners can filter by this service)
#define COLD_START_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

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

ColdStartBle::ColdStartBle() : _active(false), _startMillis(0), _connected(false) {}

void ColdStartBle::begin(esp_sleep_wakeup_cause_t wakeup_cause) {
    if (wakeup_cause != ESP_SLEEP_WAKEUP_UNDEFINED) {
        Serial.print("[ColdStartBle] Skipping BLE mode - wakeup cause: ");
        Serial.println(wakeup_cause);
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

    // Create and start service (must start service before advertising)
    NimBLEService* pService = pServer->createService(COLD_START_SERVICE_UUID);
    if (!pService) {
        NimBLEDevice::deinit(true);
        Serial.println("[ColdStartBle] ERROR: Failed to create BLE service");
        return;
    }
    if (!pService->start()) {
        NimBLEDevice::deinit(true);
        Serial.println("[ColdStartBle] ERROR: Failed to start BLE service");
        return;
    }
    Serial.println("[ColdStartBle] BLE service created and started");

    // Configure advertising: addServiceUUID then start (order: service->start(); adv->addServiceUUID(); adv->start();)
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    if (!pAdvertising) {
        NimBLEDevice::deinit(true);
        Serial.println("[ColdStartBle] ERROR: Failed to get advertising object");
        return;
    }
    
    // Set advertising parameters: advertise our custom service UUID for filtering
    pAdvertising->addServiceUUID(COLD_START_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    // setConnectableMode/setPreferredParams require NimBLE-Arduino 2.x; 1.4.x uses defaults (connectable when server present)
    pAdvertising->setMinInterval(0x20);  // 32 * 0.625ms = 20ms
    pAdvertising->setMaxInterval(0x40);  // 64 * 0.625ms = 40ms
    
    // Explicitly set device name in advertising data (important for discoverability)
    pAdvertising->setName(COLD_START_BLE_DEVICE_NAME);
    
    Serial.println("[ColdStartBle] Advertising configured:");
    Serial.print("  - Device name: ");
    Serial.println(COLD_START_BLE_DEVICE_NAME);
    Serial.print("  - Service UUID: ");
    Serial.println(COLD_START_SERVICE_UUID);
    
    // Small delay to ensure BLE stack is ready
    delay(200);

    // Start advertising (after service started and adv data set)
    if (!NimBLEDevice::startAdvertising()) {
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
