#ifndef FUN_APP_FETCH_H
#define FUN_APP_FETCH_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// API functions
String fetchMeowFact();
String fetchUselessFact();
String getEarthQuakeFact();
String getISSData();
String getRoomData();

// Helper function for Pacific Time DST calculation
bool isPacificDST(struct tm* timeinfo);

#endif // FUN_APP_FETCH_H
