#include "cold_start_ble.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#ifndef COLD_START_BLE_DEVICE_NAME
#define COLD_START_BLE_DEVICE_NAME "E-Ink Display"
#endif

// Minimal service UUID so the device is connectable and discoverable
#define COLD_START_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

/** Callback that sets a flag when a central connects. */
class ColdStartBleServerCallbacks : public BLEServerCallbacks {
public:
    explicit ColdStartBleServerCallbacks(bool* connectedFlag) : _connectedFlag(connectedFlag) {}
    void onConnect(BLEServer* /*pServer*/) override {
        *_connectedFlag = true;
    }
    void onDisconnect(BLEServer* /*pServer*/) override {}

private:
    bool* _connectedFlag;
};

ColdStartBle::ColdStartBle() : _active(false), _startMillis(0), _connected(false) {}

void ColdStartBle::begin(esp_sleep_wakeup_cause_t wakeup_cause) {
    if (wakeup_cause != ESP_SLEEP_WAKEUP_UNDEFINED) {
        return;
    }

    BLEDevice::init(COLD_START_BLE_DEVICE_NAME);

    BLEServer* pServer = BLEDevice::createServer();
    if (!pServer) {
        BLEDevice::deinit(true);
        Serial.println("[ColdStartBle] createServer failed");
        return;
    }

    pServer->setCallbacks(new ColdStartBleServerCallbacks(&_connected));

    BLEService* pService = pServer->createService(COLD_START_SERVICE_UUID);
    if (pService) {
        pService->start();
    }

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(COLD_START_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

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

    if (_connected || timeout) {
        BLEDevice::deinit(true);
        _active = false;
        if (_connected) {
            Serial.println("[ColdStartBle] BLE disabled after connection");
        } else {
            Serial.println("[ColdStartBle] BLE disabled after 60s window");
        }
    }
}
