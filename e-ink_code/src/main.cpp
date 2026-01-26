#include "main.h"
#include "api_client.h"
#include "display_manager.h"
#include "ota_manager.h"
#include <Fonts/FreeMonoBold9pt7b.h>
#include <WiFi.h>
#include "esp_sleep.h"

// SHT31 sensor instance
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// GxEPD2_290_C90c is for GDEM029C90 128x296 3-color display
GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(CS_PIN, DC_PIN, RST_PIN, BUSY_PIN));

// OTA Manager instance
OTAManager otaManager;

void initSPI() {
  // ESP32-C3 has only one SPI peripheral, so we use the default SPI instance
  // Set CS pin as OUTPUT before initializing SPI
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  
  // Initialize SPI with custom pins for ESP32-C3
  // Parameter order: SCK, MISO, MOSI, CS
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, CS_PIN);
  
  // Connect the SPI instance to the display with appropriate settings
  // 4MHz clock, MSB first, SPI mode 0
  display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  
}

void initI2C() {
  // Initialize I2C with custom pins for ESP32-C3
  Wire.begin(I2C_SDA, I2C_SCL);
  
  // Initialize SHT31 sensor
  if (sht31.begin(0x44)) { // Default I2C address is 0x44
    Serial.println("SHT31 sensor initialized successfully!");
  } else {
    Serial.println("SHT31 sensor initialization failed!");
  }
}

void initWiFi() {
  // Enable WiFi only when needed
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // Disable WiFi sleep for better performance during active use
  
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi connection failed!");
  }
}

void disableWiFi() {
  // Explicitly disconnect and turn off WiFi
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi disabled");
}


int getVoltage(){
  Serial.println("\n=== getVoltage() Debug ===");

  // Set pin as input and configure attenuation
  pinMode(V_ADC, INPUT);
  // Use ADC_ATTENDB_MAX for maximum range (~2.5V)
  analogSetPinAttenuation(V_ADC, ADC_ATTENDB_MAX);
  
  // initialize the switch pin as output
  pinMode(V_SWITCH, OUTPUT);

  Serial.println("Pin configuration:");
  Serial.println("  V_ADC pin: " + String(V_ADC));
  Serial.println("  V_SWITCH pin: " + String(V_SWITCH));
  Serial.println("  R1: " + String(R1) + " ohms");
  Serial.println("  R2: " + String(R2) + " ohms");
  Serial.println("  HIGH_VOLTAGE: " + String(HIGH_VOLTAGE) + "V");
  Serial.println("  LOW_VOLTAGE: " + String(LOW_VOLTAGE) + "V");

  // turn on the switch
  Serial.println("Turning on voltage switch...");
  digitalWrite(V_SWITCH,LOW);

  // wait for it to stabilize (not sure if this is needed)
  delay(100);

  // Read and accumulate millivolt values (this handles ADC conversion automatically)
  float voltageMilliVoltsSum = 0;
  Serial.println("Reading voltage values:");
  for (int i = 0; i < 10; i++) {
    int milliVolts = analogReadMilliVolts(V_ADC);
    voltageMilliVoltsSum += milliVolts;
    Serial.println("  Reading " + String(i+1) + "/10: " + String(milliVolts) + " mV");
    delay(10); // Small delay between readings
  }
  float voltageMilliVolts = voltageMilliVoltsSum / 10.0;
  float voltageAtADC = voltageMilliVolts / 1000.0;  // Convert mV to V
  
  Serial.println("Voltage Statistics:");
  Serial.println("  Average voltage at ADC pin: " + String(voltageMilliVolts, 2) + " mV (" + String(voltageAtADC, 3) + " V)");
  
  // Check for saturation (ESP32-C3 ADC max is ~2.5-2.6V)
  if (voltageAtADC > 2.6) {
    Serial.println("  WARNING: Voltage exceeds ADC limit! ADC may be saturated.");
  }

  // turn off the switch by setting the pin to INPUT (let it float, external pull-up will pull high)
  Serial.println("Floating voltage switch (set to INPUT, pull-up keeps high)...");
  pinMode(V_SWITCH, INPUT);
  delay(100);

  // Calculate battery voltage from voltage divider
  // Voltage divider: V_ADC = V_battery * (R2 / (R1 + R2))
  // Therefore: V_battery = V_ADC / (R2 / (R1 + R2))
  float scalingFactor = (float)R2 / (R1 + R2);
  float batteryVoltage = voltageAtADC / scalingFactor;
  
  Serial.println("\nVoltage Calculation:");
  Serial.println("  Voltage at ADC pin: " + String(voltageAtADC, 3) + "V");
  Serial.println("  Scaling factor (R2/(R1+R2)): " + String(scalingFactor, 4));
  Serial.println("  Calculated battery voltage: " + String(batteryVoltage, 3) + "V");
  
  // Warn if calculated battery voltage seems unreasonable (might indicate saturation)
  if (batteryVoltage > 4.5) {
    Serial.println("  WARNING: Calculated battery voltage > 4.5V - ADC may be saturated!");
  }
  
  // Calculate percentage based on battery voltage range (3.3V to 4.2V)
  // Linear interpolation: percentage = ((voltage - LOW_VOLTAGE) / (HIGH_VOLTAGE - LOW_VOLTAGE)) * 100
  float voltageRange = HIGH_VOLTAGE - LOW_VOLTAGE;
  float voltagePercentage = ((batteryVoltage - LOW_VOLTAGE) / voltageRange) * 100.0;
  
  // Clamp percentage between 0% and 100%
  if (voltagePercentage < 0.0) voltagePercentage = 0.0;
  if (voltagePercentage > 100.0) voltagePercentage = 100.0;
  
  Serial.println("\nResults:");
  Serial.println("  Battery voltage: " + String(batteryVoltage, 3) + "V");
  Serial.println("  Calculated percentage: " + String(voltagePercentage, 2) + "%");
  Serial.println("  Rounded percentage: " + String((int)(voltagePercentage + 0.5f)) + "%");
  Serial.println("=== End getVoltage() Debug ===\n");

  return (int)(voltagePercentage + 0.5f);
}

// Forward declarations
int checkWifiStrength();
String getWifiStatusString(wl_status_t status);

// Read temperature from SHT31 sensor
String getRoomData() {
  // Reinitialize I2C if it was disabled
  initI2C();
  
  float temperature = sht31.readTemperature();
  temperature = temperature * 9.0 / 5.0 + 32.0;
  float humidity = sht31.readHumidity();
  String result = String("Room Temp & Humidity\n");
  result += String("Temp: ") + String(temperature, 1) + "°F\n";
  result += String("Humidity: ") + String(humidity, 1) + "%";

  // Debug: Check WiFi status before initialization
  wl_status_t wifiStatusBefore = WiFi.status();
  Serial.print("[DEBUG] WiFi status before init: ");
  Serial.print(wifiStatusBefore);
  Serial.print(" (");
  Serial.print(getWifiStatusString(wifiStatusBefore));
  Serial.println(")");
  Serial.print("[DEBUG] WiFi mode before init: ");
  Serial.println(WiFi.getMode());

  // Initialize WiFi if not already connected
  if (wifiStatusBefore != WL_CONNECTED) {
    Serial.println("[DEBUG] WiFi not connected, initializing...");
    initWiFi();
  } else {
    Serial.println("[DEBUG] WiFi already connected");
  }

  // Check WiFi status after potential initialization
  wl_status_t wifiStatusAfter = WiFi.status();
  Serial.print("[DEBUG] WiFi status after init: ");
  Serial.print(wifiStatusAfter);
  Serial.print(" (");
  Serial.print(getWifiStatusString(wifiStatusAfter));
  Serial.println(")");
  
  // Only add WiFi info to display if connected
  if (wifiStatusAfter == WL_CONNECTED) {
    Serial.println("[DEBUG] WiFi is connected, getting strength...");
    int strength = checkWifiStrength();
    Serial.print("[DEBUG] Raw RSSI value: ");
    Serial.println(strength);
    
    String strengthDesc;
    if (strength > -50) {
      strengthDesc = "Excellent";
    } else if (strength >= -60 && strength <= -50) {
      strengthDesc = "Great";
    } else if (strength >= -70 && strength < -60) {
      strengthDesc = "Good";
    } else if (strength >= -80 && strength < -70) {
      strengthDesc = "Fair";
    } else if (strength >= -90 && strength < -80) {
      strengthDesc = "Weak";
    } else {
      strengthDesc = "Very Poor";
    }
    Serial.print("[DEBUG] Strength description: ");
    Serial.println(strengthDesc);
    result += String("\nWiFi:") + String(strength) + " dBm (" + strengthDesc + ")";
  }
  // If WiFi is not connected, nothing WiFi-related is added to the result
  return result;
}

// Disable all unnecessary peripherals before sleep
void disablePeripherals() {
  Serial.println("Disabling peripherals for sleep...");
  
  // Disable WiFi (handles already-disabled state)
  if (WiFi.getMode() != WIFI_OFF) {
    disableWiFi();
  }
  
  // Disable SPI (safe to call even if already disabled)
  SPI.end();
  Serial.println("SPI disabled");
  
  // Disable I2C (safe to call even if already disabled)
  Wire.end();
  Serial.println("I2C disabled");
  
  // Set SPI pins to high impedance/low power state to reduce leakage
  pinMode(SPI_SCK, INPUT);
  pinMode(SPI_MOSI, INPUT);
  pinMode(CS_PIN, INPUT);
  pinMode(DC_PIN, INPUT);
  pinMode(RST_PIN, INPUT);
  pinMode(BUSY_PIN, INPUT);
  
  // Set I2C pins to high impedance/low power state to reduce leakage
  pinMode(I2C_SDA, INPUT);
  pinMode(I2C_SCL, INPUT);
  
  Serial.println("All peripherals disabled");
}

String getWifiStatusString(wl_status_t status) {
  switch(status) {
    case WL_NO_SHIELD: return "NO_SHIELD";
    case WL_IDLE_STATUS: return "IDLE_STATUS";
    case WL_NO_SSID_AVAIL: return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED: return "SCAN_COMPLETED";
    case WL_CONNECTED: return "CONNECTED";
    case WL_CONNECT_FAILED: return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED: return "DISCONNECTED";
    default: return "UNKNOWN";
  }
}

int checkWifiStrength() {
  wl_status_t status = WiFi.status();
  Serial.print("[DEBUG] checkWifiStrength - WiFi status: ");
  Serial.println(getWifiStatusString(status));
  
  if (status != WL_CONNECTED) {
    Serial.println("[DEBUG] WiFi not connected, cannot get RSSI");
    return -100; // Return a very poor value to indicate no connection
  }
  
  int rssi = WiFi.RSSI();
  Serial.print("[DEBUG] WiFi RSSI: ");
  Serial.println(rssi);
  return rssi;
}

// Configure and enter deep sleep
void enterDeepSleep(uint64_t sleepTimeSeconds) {
  Serial.print("Entering deep sleep for ");
  Serial.print(sleepTimeSeconds);
  Serial.println(" seconds...");
  
  // Disable all peripherals before sleep
  disablePeripherals();
  
  // Flush serial output before sleep
  Serial.flush();
  delay(100);
  
  // Configure deep sleep timer
  esp_sleep_enable_timer_wakeup(sleepTimeSeconds * 1000000ULL); // Convert to microseconds
  
  // Enter deep sleep
  esp_deep_sleep_start();
  // Code never reaches here - device will restart after wake
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Print wake reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0: Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1: Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP: Serial.println("Wakeup caused by ULP program"); break;
    default: Serial.println("Wakeup was not caused by deep sleep"); break;
  }
  
  // Initialize SPI only when needed
  initSPI();
  
  // Initialize I2C and SHT31 sensor only when needed
  initI2C();
}

void loop() {
  // Static variable to track display mode across deep sleep cycles
  // Using RTC memory to persist across deep sleep
  static RTC_DATA_ATTR int displayMode = 0;
  
  // Rotate between different data sources
  // Mode 0: Room temperature and humidity (no WiFi needed)
  // Mode 1: Earthquake data (WiFi needed)
  // Mode 2: Meow fact (WiFi needed)
  // Mode 3: ISS data (WiFi needed)
  
  if (displayMode == 0) {
    // Display temperature and humidity from SHT31 sensor (no WiFi needed)
    Serial.println("Reading room data...");
    String roomData = getRoomData();
    Serial.println("Room Data: " + roomData);
    displayDefault(roomData);
  } else {
    // For API calls, initialize WiFi once at the start
    initWiFi();
    
    if (WiFi.status() == WL_CONNECTED) {
      // Initialize OTA when WiFi is connected
      otaManager.setVersionCheckUrl(OTA_VERSION_CHECK_URL);
      otaManager.setRootCA(ROOT_CA_CERT);
      otaManager.setPassword(OTA_PASSWORD);
      otaManager.setCurrentVersion(FIRMWARE_VERSION);
      otaManager.begin();
      
      // Handle OTA updates (non-blocking, checks for updates)
      otaManager.handle();
      if (displayMode == 1) {
        Serial.println("Fetching earthquake fact...");
        String fact = getEarthQuakeFact();
        Serial.println("Earthquake Fact: " + fact);
        displayEarthquakeFact(fact);
      } else if (displayMode == 2) {
        Serial.println("Fetching meow fact...");
        String fact = fetchMeowFact();
        Serial.println("Meow Fact: " + fact);
        displayDefault(fact);
      } else if (displayMode == 3) {
        Serial.println("Fetching ISS data...");
        String issData = getISSData();
        Serial.println("ISS Data: " + issData);
        displayISSData(issData);
      } else if (displayMode == 4) {
        Serial.println("Fetching useless fact...");
        String fact = fetchUselessFact();
        Serial.println("Useless Fact: " + fact);
        displayDefault(fact);
      }
    } else {
      // WiFi failed, fall back to room data
      Serial.println("WiFi not available, displaying room data...");
      String roomData = getRoomData();
      displayDefault(roomData);
    }
    
    // Disable WiFi after all API calls are done
    disableWiFi();
  }
  
  // Disable I2C after reading sensor (we'll reinitialize if needed)
  Wire.end();
  Serial.println("I2C disabled after sensor read");
  
  // Disable SPI after display update (we'll reinitialize if needed)
  SPI.end();
  Serial.println("SPI disabled after display update");
  
  // Cycle to next display mode (0-3, then back to 0)
  displayMode = (displayMode + 1) % 5;

  // Handle OTA updates before entering sleep
  // If OTA update is in progress, stay awake and handle it
  if (WiFi.status() == WL_CONNECTED) {
    otaManager.handle();
    
    // If OTA update is in progress, don't sleep - keep handling updates
    if (otaManager.isUpdating()) {
      Serial.println("[OTA] Update in progress, staying awake...");
      while (otaManager.isUpdating()) {
        otaManager.handle();
        delay(100);
      }
      Serial.println("[OTA] Update complete, restarting...");
      delay(1000);
      ESP.restart();
      return; // Never reached, but good practice
    }
  }

  // 30000 milliseconds is 30 seconds, which is 0.5 minutes
  delay(150000); // 5 minutes
  
  // Enter deep sleep for 5 minutes (300 seconds)
  // On wake, the device will restart and run setup() again
  // displayMode will persist in RTC memory
  //enterDeepSleep(300);
  
  // Code never reaches here due to deep sleep restart
}
