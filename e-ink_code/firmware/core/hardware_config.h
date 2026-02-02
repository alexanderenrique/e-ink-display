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
#define BATTERY_LOW_VOLTAGE  3.3f

// ============================================================================
// Network Configuration
// ============================================================================
// WiFi credentials
#define WIFI_SSID            "Zucotti Manicotti"
#define WIFI_PASSWORD        "100BoiledEggs"

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
