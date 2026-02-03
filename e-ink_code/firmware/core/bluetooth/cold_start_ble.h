#ifndef COLD_START_BLE_H
#define COLD_START_BLE_H

#include <Arduino.h>
#include <esp_sleep.h>

/**
 * Enables Bluetooth for a short window only on cold start (power-on reset).
 * Does nothing when waking from deep sleep (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED).
 *
 * BLE stays on for up to COLD_START_BLE_WINDOW_SECONDS, or until a central connects, then is disabled.
 */
class ColdStartBle {
public:
    static const uint32_t COLD_START_BLE_WINDOW_SECONDS = 60;

    ColdStartBle();

    /**
     * Call from setup(). If wakeup_cause is ESP_SLEEP_WAKEUP_UNDEFINED (cold boot),
     * initializes BLE and starts advertising. Otherwise does nothing.
     */
    void begin(esp_sleep_wakeup_cause_t wakeup_cause);

    /**
     * Call from loop(). When active, checks whether 60s elapsed or a device connected;
     * when so, deinits BLE and clears active state.
     */
    void loop();

    /** True when BLE window is active (cold start and not yet timed out / connected). */
    bool isActive() const { return _active; }

private:
    bool _active;
    uint32_t _startMillis;
    bool _connected;
};

#endif // COLD_START_BLE_H
