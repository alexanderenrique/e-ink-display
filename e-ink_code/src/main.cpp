#include <Arduino.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <string.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include "esp_sleep.h"

// WiFi credentials - update these with your network details
#define WIFI_SSID "Zucotti Manicotti"
#define WIFI_PASSWORD "100BoiledEggs"

// SPI pin configuration for ESP32-C3
#define SPI_SCK   3   // Serial Clock
#define SPI_MOSI  4   // Master Out Slave In (data to display)
#define SPI_MISO  -1  // Master In Slave Out (not used for e-ink displays)

// Display configuration for 296x128 e-ink display (3-color: red, black, white)
#define RST_PIN   5
#define DC_PIN    6
#define CS_PIN    7
#define BUSY_PIN  21

// I2C pin configuration for SHT31 sensor
#define I2C_SDA   9   // Data line
#define I2C_SCL   10  // Clock line

// Voltage Divider Pins
#define V_ADC 2
#define V_SWITCH 8
#define R1 47000
#define R2 68000
#define HIGH_VOLTAGE 4.2
#define LOW_VOLTAGE 3.3

// SHT31 sensor instance
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// API endpoint
const char* meowfacts_url = "https://meowfacts.herokuapp.com/";
const char* earthquake_url = "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/2.5_day.geojson";
const char* iss_url = "https://api.wheretheiss.at/v1/satellites/25544";
const char* uselessfacts_url = "https://uselessfacts.jsph.pl/api/v2/facts/random?language=en";
// GxEPD2_290_C90c is for GDEM029C90 128x296 3-color display
GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(CS_PIN, DC_PIN, RST_PIN, BUSY_PIN));

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

String fetchMeowFact() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    return "WiFi Error";
  }
  
  HTTPClient http;
  http.begin(meowfacts_url);
  
  int httpCode = http.GET();
  String fact = "Error fetching fact";
  
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Raw response: " + payload);
    
    // Clean up payload - remove any trailing characters like %
    payload.trim();
    
    // Parse JSON: {"data":["fact text here"]}
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
    } else if (doc.containsKey("data") && doc["data"].is<JsonArray>() && doc["data"].size() > 0) {
      fact = doc["data"][0].as<String>();
      fact.trim(); // Clean up the fact text
      fact = String("Cat Facts\n") + fact;
      Serial.println("Parsed fact: " + fact);
    } else {
      Serial.println("Unexpected JSON structure");
    }
  } else {
    Serial.printf("HTTP request failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
  return fact;
}

String fetchUselessFact() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    return "WiFi Error";
  }
  
  HTTPClient http;
  http.begin(uselessfacts_url);
  
  int httpCode = http.GET();
  String fact = "Error fetching fact";
  
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Raw response: " + payload);
    
    // Clean up payload
    payload.trim();
    
    // Parse JSON: {"id":"...","text":"fact text here","source":"...","source_url":"...","language":"en","permalink":"..."}
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
    } else if (doc.containsKey("text")) {
      fact = doc["text"].as<String>();
      fact.trim(); // Clean up the fact text
      fact = String("Fun Fact!\n") + fact;
      Serial.println("Parsed fact: " + fact);
    } else {
      Serial.println("Unexpected JSON structure");
    }
  } else {
    Serial.printf("HTTP request failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
  return fact;
}

// Helper function to determine if DST is in effect for Pacific Time
// DST runs from 2nd Sunday in March (approx March 8-14) to 1st Sunday in November (approx Nov 1-7)
bool isPacificDST(struct tm* timeinfo) {
  if (timeinfo == nullptr) return false;
  
  int month = timeinfo->tm_mon + 1; // tm_mon is 0-11
  int day = timeinfo->tm_mday;
  
  // Before March 8: PST (no DST)
  if (month < 3 || (month == 3 && day < 8)) return false;
  
  // After November 7: PST (no DST)
  if (month > 11 || (month == 11 && day > 7)) return false;
  
  // March 8 through November 7: DST
  return true;
}

String getEarthQuakeFact(){
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    return "WiFi Error";
  }
  
  HTTPClient http;
  http.begin(earthquake_url);
  
  int httpCode = http.GET();
  String result = "Error fetching earthquake data";
  
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Raw response length: " + String(payload.length()));
    
    // Clean up payload
    payload.trim();
    
    // Parse GeoJSON: {"type":"FeatureCollection","features":[...]}
    // Need larger buffer for earthquake data (32KB should be enough)
    DynamicJsonDocument doc(32768);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
    } else if (doc.containsKey("features") && doc["features"].is<JsonArray>() && doc["features"].size() > 0) {
      // Get the first (latest) earthquake
      JsonObject feature = doc["features"][0];
      JsonObject properties = feature["properties"];
      
      // Extract magnitude, location, and time
      float magnitude = properties["mag"].as<float>();
      String place = properties["place"].as<String>();
      unsigned long long timeUnixMs = properties["time"].as<unsigned long long>();
      
      // Convert Unix timestamp (milliseconds) to readable time
      // timeUnixMs is in milliseconds, so divide by 1000 for seconds
      time_t timeSeconds = (time_t)(timeUnixMs / 1000);
      
      // First calculate Pacific time assuming PST (UTC-8)
      time_t pacificTimeSeconds = timeSeconds - (8 * 3600);
      struct tm *pacificTime = gmtime(&pacificTimeSeconds);
      
      // Check if DST is in effect based on Pacific time
      // If yes, recalculate with PDT offset (UTC-7)
      bool isDST = false;
      if (pacificTime != nullptr) {
        isDST = isPacificDST(pacificTime);
        if (isDST) {
          pacificTimeSeconds = timeSeconds - (7 * 3600);
          pacificTime = gmtime(&pacificTimeSeconds);
        }
      }
      
      // Format: "M 4.6 - Location\nTime"
      char timeStr[64];
      if (pacificTime != nullptr) {
        const char* tzLabel = isDST ? "PDT" : "PST";
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", pacificTime);
        snprintf(timeStr + strlen(timeStr), sizeof(timeStr) - strlen(timeStr), " %s", tzLabel);
      } else {
        // Fallback: show Unix timestamp
        snprintf(timeStr, sizeof(timeStr), "Time: %llu", timeUnixMs);
      }
      
      result = String("Latest Earthquake\n");
      result += "M " + String(magnitude, 1) + " - " + place + "\n" + String(timeStr);
      
      Serial.println("Latest earthquake:");
      Serial.println("  Magnitude: " + String(magnitude));
      Serial.println("  Location: " + place);
      Serial.println("  Time: " + String(timeStr));
    } else {
      Serial.println("Unexpected JSON structure or no earthquakes found");
    }
  } else {
    Serial.printf("HTTP request failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
  return result;
}

String getISSData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    return "WiFi Error";
  }
  
  HTTPClient http;
  http.begin(iss_url);
  
  int httpCode = http.GET();
  String result = "Error fetching ISS data";
  
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Raw ISS response: " + payload);
    
    // Clean up payload
    payload.trim();
    
    // Parse JSON
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
    } else {
      // Extract ISS data
      float latitude = doc["latitude"].as<float>();
      float longitude = doc["longitude"].as<float>();
      float altitude_km = doc["altitude"].as<float>();
      float velocity_kph = doc["velocity"].as<float>();
      
      // Convert to miles
      float altitude_miles = altitude_km * 0.621371;
      float velocity_mph = velocity_kph * 0.621371;
      
      // Format: "Where is the ISS?\nLat, Long\nAlt KM / Miles\nVel KPH / MPH"
      result = String("Where is the ISS?\n");
      result += String("Lat/Long: ") + String(latitude, 2) + ", " + String(longitude, 2) + "\n";
      result += String("Altitude: ") + String(altitude_miles, 2) + " mi\n";
      result += String("Velocity: ") + String(velocity_mph, 2) + " mph\n";

    }
  } else {
    Serial.printf("HTTP request failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
  return result;
}

// Helper function to render text with word wrapping
// Returns the final Y position after rendering
int renderTextWithWrap(String text, int startX, int startY, int maxWidth, int lineHeight, uint16_t textColor) {
  int yPos = startY;
  int xPos = startX;
  String word = "";
  
  // First, collect all words into an array for better look-ahead
  String words[100]; // Max 100 words
  bool isNewline[100]; // Track which entries are newlines
  int wordCount = 0;
  int wordStart = 0;
  
  for (int i = 0; i <= text.length(); i++) {
    char c = (i < text.length()) ? text.charAt(i) : ' ';
    if (c == '\n' || c == ' ') {
      if (i > wordStart && wordCount < 100) {
        words[wordCount] = text.substring(wordStart, i);
        isNewline[wordCount] = false;
        wordCount++;
      }
      if (c == '\n' && wordCount < 100) {
        words[wordCount] = ""; // Empty string marks newline
        isNewline[wordCount] = true;
        wordCount++;
      }
      wordStart = i + 1;
    }
  }
  
  // Now render words with smart wrapping
  for (int i = 0; i < wordCount; i++) {
    if (isNewline[i]) {
      // Handle explicit newline
      yPos += lineHeight;
      xPos = startX;
      continue;
    }
    
    String currentWord = words[i];
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(currentWord, xPos, yPos, &x1, &y1, &w, &h);
    
    // Check if current word fits on current line
    bool fitsOnCurrentLine = (xPos + w <= maxWidth);
    
    // Look ahead to next word to avoid orphaned short words
    bool shouldWrap = false;
    if (!fitsOnCurrentLine && xPos > startX) {
      // Word doesn't fit, need to wrap
      shouldWrap = true;
    } else if (fitsOnCurrentLine && xPos > startX && i + 1 < wordCount && !isNewline[i + 1]) {
      // Word fits, but check if next word would also fit
      String nextWord = words[i + 1];
      int16_t nx1, ny1;
      uint16_t nw, nh;
      display.getTextBounds(nextWord, xPos + w + 5, yPos, &nx1, &ny1, &nw, &nh);
      
      // If next word wouldn't fit, and current word is short (<= 4 chars), wrap both
      if (xPos + w + 5 + nw > maxWidth && currentWord.length() <= 4) {
        shouldWrap = true;
      }
    }
    
    if (shouldWrap) {
      yPos += lineHeight;
      xPos = startX;
      // Recalculate bounds at new position
      display.getTextBounds(currentWord, xPos, yPos, &x1, &y1, &w, &h);
    }
    
    display.setCursor(xPos, yPos);
    display.print(currentWord);
    xPos += w + 5; // Space between words
  }
  
  // Return final Y position (add lineHeight for next line)
  return yPos + lineHeight;
}

// Forward declaration
void displayBatteryPercentage();

// Display function specifically for earthquake facts
// Format: "Latest Earthquake\nM 4.6 - Location\nDate Time PST/PDT"
void displayEarthquakeFact(String earthquakeData) {
  // Reinitialize SPI if it was disabled
  initSPI();
  display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200, true, 2, false);
  display.setRotation(-1); // Landscape orientation
  display.setFont(&FreeMonoBold9pt7b);
  
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // Display battery percentage in upper right corner
    displayBatteryPercentage();
    
    // Parse the earthquake data (format: "Latest Earthquake\nM X.X - Location\nDate Time TZ")
    int yPos = 20;
    int lineHeight = 25;
    int startX = 10;
    int maxWidth = 280;
    
    // Split by newlines
    int pos = 0;
    int lineNum = 0;
    while (pos < earthquakeData.length()) {
      int newlinePos = earthquakeData.indexOf('\n', pos);
      String line = "";
      if (newlinePos == -1) {
        line = earthquakeData.substring(pos);
        pos = earthquakeData.length();
      } else {
        line = earthquakeData.substring(pos, newlinePos);
        pos = newlinePos + 1;
      }
      
      // First line (title) in red, rest in black
      if (lineNum == 0) {
        display.setTextColor(GxEPD_RED);
      } else {
        display.setTextColor(GxEPD_BLACK);
      }
      
      int finalY = renderTextWithWrap(line, startX, yPos, maxWidth, lineHeight, 
                                       (lineNum == 0) ? GxEPD_RED : GxEPD_BLACK);
      yPos = finalY;
      lineNum++;
    }
  } while (display.nextPage());
  
  display.hibernate();
}

int getVoltage(){
  Serial.println("\n=== getVoltage() Debug ===");
  
  // Configure ADC pin attenuation for ESP32-C3
  // IMPORTANT: ESP32-C3 ADC can only measure up to ~2.5-2.6V max (ADC_ATTEN_DB_11)
  // Any voltage above ~2.6V will saturate to 4095 (max ADC value)
  // Voltage divider (R1=47k, R2=68k) scales battery voltage down:
  //   Scaling factor = R2/(R1+R2) = 68000/115000 = 0.5913
  //   4.2V battery → 2.48V at ADC (safe, under 2.5V limit)
  //   3.3V battery → 1.95V at ADC (safe, well under limit)
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

// Helper function to display battery percentage in upper right corner in red
void displayBatteryPercentage() {
  int batteryPercent = getVoltage();
  String batteryText = String(batteryPercent) + "%";
  
  // Get text bounds to position in upper right corner
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(batteryText, 0, 0, &x1, &y1, &w, &h);
  
  // Position in upper right corner with padding (10 pixels from right edge, 10 pixels from top)
  int displayWidth = display.width();
  int xPos = displayWidth - w - 10;
  int yPos = 10;
  
  // Display in red
  display.setTextColor(GxEPD_RED);
  display.setCursor(xPos, yPos);
  display.print(batteryText);
}

// Display function for ISS data
void displayISSData(String issData) {
  // Reinitialize SPI if it was disabled
  initSPI();
  display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200, true, 2, false);
  display.setRotation(1); // Landscape orientation
  display.setFont(&FreeMonoBold9pt7b);
  
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // Display battery percentage in upper right corner
    displayBatteryPercentage();
    
    // Parse the ISS data (format: "Where is the ISS?\nLat, Long\nAlt KM / Miles\nVel KPH / MPH")
    int yPos = 20;
    int lineHeight = 25;
    int startX = 10;
    int maxWidth = 280;
    
    // Split by newlines
    int pos = 0;
    int lineNum = 0;
    while (pos < issData.length()) {
      int newlinePos = issData.indexOf('\n', pos);
      String line = "";
      if (newlinePos == -1) {
        line = issData.substring(pos);
        pos = issData.length();
      } else {
        line = issData.substring(pos, newlinePos);
        pos = newlinePos + 1;
      }
      
      // First line in red, rest in black
      if (lineNum == 0) {
        display.setTextColor(GxEPD_RED);
      } else {
        display.setTextColor(GxEPD_BLACK);
      }
      
      display.setCursor(startX, yPos);
      display.print(line);
      yPos += lineHeight;
      lineNum++;
    }
  } while (display.nextPage());
  
  display.hibernate();
}

// Default display function for general text
// First line in red, rest in black
void displayDefault(String fact) {
  // Reinitialize SPI if it was disabled
  initSPI();
  display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200, true, 2, false);
  display.setRotation(1); // Landscape orientation
  display.setFont(&FreeMonoBold9pt7b);
  
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // Display battery percentage in upper right corner
    displayBatteryPercentage();
    
    // Split by first newline to separate first line from rest
    int newlinePos = fact.indexOf('\n');
    String firstLine = "";
    String restOfText = "";
    
    if (newlinePos > 0) {
      firstLine = fact.substring(0, newlinePos);
      restOfText = fact.substring(newlinePos + 1);
    } else {
      firstLine = fact;
    }
    
    // Display first line in red
    display.setTextColor(GxEPD_RED);
    int finalY = renderTextWithWrap(firstLine, 10, 20, 280, 25, GxEPD_RED);
    
    // Display rest of text in black
    if (restOfText.length() > 0) {
      display.setTextColor(GxEPD_BLACK);
      renderTextWithWrap(restOfText, 10, finalY, 280, 25, GxEPD_BLACK);
    }
  } while (display.nextPage());
  
  display.hibernate();
}

// Generic display function (fallback)
void displayFact(String fact) {
  displayDefault(fact);
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
  // displayMode = (displayMode + 1) % 5;

  // 30000 milliseconds is 30 seconds, which is 0.5 minutes
  delay(30000); // 30 seconds
  
  // Enter deep sleep for 5 minutes (300 seconds)
  // On wake, the device will restart and run setup() again
  // displayMode will persist in RTC memory
  //enterDeepSleep(300);
  
  // Code never reaches here due to deep sleep restart
}
