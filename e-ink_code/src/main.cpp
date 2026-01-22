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

// WiFi credentials - update these with your network details
#define WIFI_SSID "Zucotti Manicotti"
#define WIFI_PASSWORD "100BoiledEggs"

// SPI pin configuration for ESP32-C3
#define SPI_SCK   2   // Serial Clock
#define SPI_MOSI  3   // Master Out Slave In (data to display)
#define SPI_MISO  -1  // Master In Slave Out (not used for e-ink displays)

// Display configuration for 296x128 e-ink display (3-color: red, black, white)
#define RST_PIN   4
#define DC_PIN    5
#define CS_PIN    6
#define BUSY_PIN  7

// I2C pin configuration for SHT31 sensor
#define I2C_SDA   9   // Data line
#define I2C_SCL   10  // Clock line

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
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
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
      
      result = "M " + String(magnitude, 1) + " - " + place + "\n" + String(timeStr);
      
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
  
  for (int i = 0; i < text.length(); i++) {
    char c = text.charAt(i);
    if (c == '\n') {
      // Handle newline: print current word and move to next line
      if (word.length() > 0) {
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(word, xPos, yPos, &x1, &y1, &w, &h);
        display.setCursor(xPos, yPos);
        display.print(word);
        word = "";
      }
      // Move to next line
      yPos += lineHeight;
      xPos = startX;
    } else if (c == ' ') {
      if (word.length() > 0) {
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(word, xPos, yPos, &x1, &y1, &w, &h);
        
        if (xPos + w > maxWidth && xPos > startX) {
          yPos += lineHeight; // New line
          xPos = startX;
        }
        
        display.setCursor(xPos, yPos);
        display.print(word);
        xPos += w + 5; // Space between words
        word = "";
      }
    } else {
      word += c;
    }
  }
  
  // Print last word
  if (word.length() > 0) {
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(word, xPos, yPos, &x1, &y1, &w, &h);
    
    if (xPos + w > maxWidth && xPos > startX) {
      yPos += lineHeight;
      xPos = startX;
    }
    
    display.setCursor(xPos, yPos);
    display.print(word);
  }
  
  // Return final Y position (add lineHeight for next line)
  return yPos + lineHeight;
}

// Display function specifically for earthquake facts
// Format: "M 4.6 - Location\nDate Time PST/PDT"
void displayEarthquakeFact(String earthquakeData) {
  // Ensure SPI is connected before initializing display
  display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200, true, 2, false);
  display.setRotation(-1); // Landscape orientation
  display.setFont(&FreeMonoBold9pt7b);
  
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // Parse the earthquake data (format: "M X.X - Location\nDate Time TZ")
    int newlinePos = earthquakeData.indexOf('\n');
    String magnitudeAndLocation = "";
    String dateTime = "";
    
    if (newlinePos > 0) {
      magnitudeAndLocation = earthquakeData.substring(0, newlinePos);
      dateTime = earthquakeData.substring(newlinePos + 1);
    } else {
      magnitudeAndLocation = earthquakeData;
    }
    
    // Display magnitude and location in red
    display.setTextColor(GxEPD_RED);
    int finalY = renderTextWithWrap(magnitudeAndLocation, 10, 20, 280, 25, GxEPD_RED);
    
    // Display date/time in black, positioned after the magnitude/location
    if (dateTime.length() > 0) {
      display.setTextColor(GxEPD_BLACK);
      renderTextWithWrap(dateTime, 10, finalY, 280, 25, GxEPD_BLACK);
    }
  } while (display.nextPage());
  
  display.hibernate();
}

// Display function for ISS data
void displayISSData(String issData) {
  // Ensure SPI is connected before initializing display
  display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200, true, 2, false);
  display.setRotation(1); // Landscape orientation
  display.setFont(&FreeMonoBold9pt7b);
  
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
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
  // Ensure SPI is connected before initializing display
  display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200, true, 2, false);
  display.setRotation(1); // Landscape orientation
  display.setFont(&FreeMonoBold9pt7b);
  
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
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

// Read temperature from SHT31 sensor
String getRoomData() {
  float temperature = sht31.readTemperature();
  temperature = temperature * 9.0 / 5.0 + 32.0;
  float humidity = sht31.readHumidity();
  String result = String("Room Temp & Humidity\n");
  result += String("Temp: ") + String(temperature, 1) + "°F\n";
  result += String("Humidity: ") + String(humidity, 1) + "%";
  return result;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize SPI
  initSPI();
  
  // Initialize I2C and SHT31 sensor
  initI2C();
  
  // Initialize WiFi
  // initWiFi();
  
  Serial.println("Updating display...");
  displayDefault("Hi Jeff");
  Serial.println("Display updated!");
}

void loop() {
  // First delay is 60 seconds, then 10 minutes between updates
  static bool firstIteration = true;
  
  if (firstIteration) {
    delay(6000); // 6 seconds before first fact
    firstIteration = false;
  } else {
    delay(300000); // 5 minutes between subsequent facts
  }
  
  //if (WiFi.status() == WL_CONNECTED) {
    // Rotate between earthquake facts, meow facts, and ISS data
    // static int displayMode = 0;
    // displayMode = (displayMode + 1) % 3;
    
    // if (displayMode == 0) {
    //   Serial.println("Fetching earthquake fact...");
    //   String fact = getEarthQuakeFact();
    //   Serial.println("Earthquake Fact: " + fact);
    //   displayEarthquakeFact(fact);
    // } else if (displayMode == 1) {
    //   Serial.println("Fetching meow fact...");
    //   String fact = fetchMeowFact();
    //   Serial.println("Meow Fact: " + fact);
    //   displayDefault(fact);
    // } else {
      // Serial.println("Fetching ISS data...");
      // String issData = getISSData();
      // Serial.println("ISS Data: " + issData);
      // displayISSData(issData);
      // String uselessFact = fetchUselessFact();
      // displayDefault(uselessFact);
      
  // Display temperature and humidity from SHT31 sensor
  Serial.println("Reading room data...");
  String roomData = getRoomData();
  Serial.println("Room Data: " + roomData);
  displayDefault(roomData);
}
