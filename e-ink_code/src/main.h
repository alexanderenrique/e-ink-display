#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>
#include <GxEPD2_3C.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <WiFi.h>

// Pin definitions
#define SPI_SCK   21
#define SPI_MOSI  7
#define SPI_MISO  -1
#define RST_PIN   6
#define DC_PIN    5
#define CS_PIN    4
#define BUSY_PIN  3
#define I2C_SDA   9
#define I2C_SCL   10
#define V_ADC 2
#define V_SWITCH 8
#define R1 47000
#define R2 68000
#define HIGH_VOLTAGE 4.15
#define LOW_VOLTAGE 3.3

// WiFi credentials
#define WIFI_SSID "Zucotti Manicotti"
#define WIFI_PASSWORD "100BoiledEggs"

// OTA Update Server Configuration
#define OTA_VERSION_CHECK_URL "https://your-server.com/api/version"
#define OTA_PASSWORD "your-secure-password-here"
#define FIRMWARE_VERSION "1.0.0"

// Root CA Certificate (paste your server's root CA cert here)
#define ROOT_CA_CERT \
"-----BEGIN CERTIFICATE-----\n" \
"YOUR_ROOT_CA_CERTIFICATE_HERE\n" \
"-----END CERTIFICATE-----\n"

// Forward declarations
void initSPI();
void initI2C();
void initWiFi();
void disableWiFi();
int getVoltage();
String getRoomData();
void disablePeripherals();
void enterDeepSleep(uint64_t sleepTimeSeconds);
int checkWifiStrength();
String getWifiStatusString(wl_status_t status);

// Forward declaration for OTA manager
class OTAManager;
extern OTAManager otaManager;

// External sensor instance
extern Adafruit_SHT31 sht31;

#endif // MAIN_H
