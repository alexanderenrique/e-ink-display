#ifndef COLD_START_BLE_H
#define COLD_START_BLE_H

#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_system.h>

/**
 * Enables Bluetooth for a short window only on cold start (power-on reset).
 * Does nothing when waking from deep sleep (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED).
 *
 * BLE stays on for up to COLD_START_BLE_WINDOW_SECONDS (15 s), or until config is received (then device restarts).
 * When a central connects, BLE remains active so it can discover services and send config.
 */
class ColdStartBle {
public:
    static const uint32_t COLD_START_BLE_WINDOW_SECONDS = 15;  // 15 seconds

    ColdStartBle();

    /**
     * Call from setup().
     *
     * Enables BLE on true cold starts:
     * - Deep-sleep wakeup cause is ESP_SLEEP_WAKEUP_UNDEFINED, OR
     * - Reset reason indicates power-on/brownout.
     *
     * Otherwise (e.g., deep sleep timer wake) does nothing.
     */
    void begin(esp_sleep_wakeup_cause_t wakeup_cause, esp_reset_reason_t reset_reason, bool skip_ble);

    /**
     * Call from loop(). When active, checks whether the window elapsed or a device connected;
     * when so, deinits BLE and clears active state.
     */
    void loop();

    /** True when BLE window is active (cold start and not yet timed out / connected). */
    bool isActive() const { return _active; }

    /**
     * Get stored WiFi SSID from Preferences.
     * Returns empty string if not set.
     */
    static String getStoredWiFiSSID();

    /**
     * Get stored WiFi password from Preferences.
     * Returns empty string if not set.
     */
    static String getStoredWiFiPassword();

    /**
     * Get stored configuration JSON string from Preferences.
     * Returns empty string if not set.
     */
    static String getStoredConfigJson();

    /**
     * Check if configuration is stored in Preferences.
     */
    static bool hasStoredConfig();

    /**
     * Check if BLE should be skipped on this boot (e.g., after config-triggered restart).
     * Clears the flag after checking.
     */
    static bool shouldSkipBle();

private:
    bool _active;
    uint32_t _startMillis;
    bool _connected;
};

#endif // COLD_START_BLE_H
