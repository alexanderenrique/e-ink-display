#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// API endpoints
extern const char* meowfacts_url;
extern const char* earthquake_url;
extern const char* iss_url;
extern const char* uselessfacts_url;

// API functions
String fetchMeowFact();
String fetchUselessFact();
String getEarthQuakeFact();
String getISSData();

// Helper function for Pacific Time DST calculation
bool isPacificDST(struct tm* timeinfo);

#endif // API_CLIENT_H
