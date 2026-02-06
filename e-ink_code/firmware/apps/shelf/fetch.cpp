#include "fetch.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

String fetchShelfData(const char* binId, const char* serverUrl) {
    if (WiFi.status() != WL_CONNECTED) {
        return "WiFi Error\nNot connected";
    }
    
    if (binId == nullptr || *binId == '\0') {
        return "Config Error\nBin ID not set";
    }
    
    if (serverUrl == nullptr || *serverUrl == '\0') {
        return "Config Error\nServer URL not set";
    }
    
    // Build request URL: http://server:port/bin/<binId>
    String url = String(serverUrl);
    if (!url.endsWith("/")) {
        url += "/";
    }
    url += "bin/";
    url += binId;
    
    Serial.print("[ShelfApp] Fetching bin info from: ");
    Serial.println(url);
    
    HTTPClient http;
    if (!http.begin(url)) {
        Serial.println("[ShelfApp] fetchShelfData: http.begin failed");
        return "API Error\nFailed to connect";
    }
    
    // Set timeout
    http.setTimeout(10000);  // 10 second timeout
    
    int httpCode = http.GET();
    
    if (httpCode <= 0) {
        Serial.printf("[ShelfApp] fetchShelfData: HTTP GET failed, error: %s\n", 
                      http.errorToString(httpCode).c_str());
        http.end();
        return "API Error\nConnection failed";
    }
    
    if (httpCode == 404) {
        Serial.printf("[ShelfApp] fetchShelfData: Bin '%s' not found\n", binId);
        http.end();
        return "Bin Not Found\nID: " + String(binId);
    }
    
    if (httpCode < 200 || httpCode >= 300) {
        Serial.printf("[ShelfApp] fetchShelfData: HTTP error %d\n", httpCode);
        String response = http.getString();
        Serial.println("Response: " + response);
        http.end();
        return "API Error\nHTTP " + String(httpCode);
    }
    
    String payload = http.getString();
    http.end();
    
    // Parse JSON response from server
    // Expected format: {"bin_id": "...", "owner": {"name": "...", "email": "..."}, ...}
    DynamicJsonDocument doc(2048);  // 2KB should be enough for a single bin response
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.print("[ShelfApp] fetchShelfData: JSON parse error: ");
        Serial.println(error.c_str());
        Serial.println("Payload: " + payload);
        return "API Error\nInvalid JSON";
    }
    
    // Build display string
    String result = "Bin: " + String(binId) + "\n";
    
    // Check if owner exists
    if (doc.containsKey("owner") && !doc["owner"].isNull()) {
        JsonObject owner = doc["owner"];
        
        // Get owner name
        String ownerName;
        if (owner.containsKey("name")) {
            ownerName = owner["name"].as<String>();
        } else if (owner.containsKey("username")) {
            ownerName = owner["username"].as<String>();
        } else {
            ownerName = "User " + String(owner["id"].as<int>());
        }
        
        result += "Owner: " + ownerName + "\n";
        
        // Add email if available
        if (owner.containsKey("email")) {
            String email = owner["email"].as<String>();
            if (email.length() > 0) {
                result += email;
            }
        }
    } else {
        result += "No owner assigned";
    }
    
    // Add bin name if available and different from ID
    if (doc.containsKey("bin_name")) {
        String binName = doc["bin_name"].as<String>();
        if (binName.length() > 0 && binName != binId) {
            result += "\n" + binName;
        }
    }
    
    Serial.println("[ShelfApp] fetchShelfData: Success");
    Serial.println("Result: " + result);
    
    return result;
}
