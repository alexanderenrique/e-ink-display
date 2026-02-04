#include "fetch.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_SHT31.h>

// SHT31 sensor instance
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// API endpoints
const char* meowfacts_url = "https://meowfacts.herokuapp.com/";
const char* earthquake_url = "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/2.5_day.geojson";
const char* iss_url = "https://api.wheretheiss.at/v1/satellites/25544";
const char* uselessfacts_url = "https://uselessfacts.jsph.pl/api/v2/facts/random?language=en";

void initI2C() {
    // Initialize I2C with custom pins for ESP32-C3
    Wire.begin(I2C_SDA, I2C_SCL);
    
    // Initialize SHT31 sensor
    if (sht31.begin(0x44)) { // Default I2C address is 0x44
    } else {
        Serial.println("SHT31 sensor initialization failed!");
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

String getRoomData() {
    // Reinitialize I2C if it was disabled
    initI2C();
    
    float temperature = sht31.readTemperature();
    temperature = temperature * 9.0 / 5.0 + 32.0;
    float humidity = sht31.readHumidity();
    String result = String("Room Temp & Humidity\n");
    result += String("Temp: ") + String(temperature, 1) + "°F\n";
    result += String("Humidity: ") + String(humidity, 1) + "%";
    
    // Add WiFi info if connected
    if (WiFi.status() == WL_CONNECTED) {
        int rssi = WiFi.RSSI();
        String strengthDesc;
        if (rssi > -50) {
            strengthDesc = "Excellent";
        } else if (rssi >= -60 && rssi <= -50) {
            strengthDesc = "Great";
        } else if (rssi >= -70 && rssi < -60) {
            strengthDesc = "Good";
        } else if (rssi >= -80 && rssi < -70) {
            strengthDesc = "Fair";
        } else if (rssi >= -90 && rssi < -80) {
            strengthDesc = "Weak";
        } else {
            strengthDesc = "Very Poor";
        }
        result += String("\nWiFi:") + String(rssi) + " dBm (" + strengthDesc + ")";
    }
    
    return result;
}
