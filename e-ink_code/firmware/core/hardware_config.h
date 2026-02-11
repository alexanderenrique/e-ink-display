#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

// ============================================================================
// Pin Definitions for ESP32-C3
// ============================================================================
#define SPI_SCK   21
#define SPI_MOSI  7
#define SPI_MISO  -1
#define RST_PIN   6
#define DC_PIN    5
#define CS_PIN    4
#define BUSY_PIN  3
#define I2C_SDA   9
#define I2C_SCL   10
#define V_ADC     2
#define V_SWITCH  8

// ============================================================================
// Power Management Constants
// ============================================================================
// Voltage divider resistor values (ohms)
#define BATTERY_R1           47000
#define BATTERY_R2           68000

// Battery voltage range for percentage calculation
#define BATTERY_HIGH_VOLTAGE 4.15f
#define BATTERY_LOW_VOLTAGE  2.8f

// Set to 1 to use delay() instead of deep sleep (keeps Serial and USB alive for testing)
#define DISABLE_DEEP_SLEEP_FOR_TESTING 0

// Battery management thresholds
#define BATTERY_LOW_THRESHOLD_PERCENT  5   // Enter low battery sleep mode at this level
#define BATTERY_RESUME_THRESHOLD_PERCENT 15  // Resume normal operation when battery reaches this level
#define LOW_BATTERY_WAKEUP_INTERVAL_SECONDS 300  // Wake up every 5 minutes to check battery when in low battery mode

// ============================================================================
// Network Configuration
// ============================================================================
// WiFi credentials are now configured via BLE and stored in Preferences
// Use ColdStartBle::getStoredWiFiSSID() and ColdStartBle::getStoredWiFiPassword()
// to retrieve them at runtime.

// OTA Update Server Configuration
#define OTA_VERSION_CHECK_URL "https://your-server.com/api/version"
#define OTA_PASSWORD          "your-secure-password-here"
#define FIRMWARE_VERSION      "1.0.0"

// Root CA Certificate (paste your server's root CA cert here)
#define ROOT_CA_CERT \
"-----BEGIN CERTIFICATE-----\n" \
"YOUR_ROOT_CA_CERTIFICATE_HERE\n" \
"-----END CERTIFICATE-----\n"

#endif // HARDWARE_CONFIG_H
