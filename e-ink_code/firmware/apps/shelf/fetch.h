#ifndef SHELF_APP_FETCH_H
#define SHELF_APP_FETCH_H

#include <Arduino.h>

// Shelf data fetching functions
// binId: The bin ID to look up (can be numeric string or name)
// serverUrl: Base URL of the bin lookup server (e.g., "http://192.168.1.100:8080")
String fetchShelfData(const char* binId, const char* serverUrl);

#endif // SHELF_APP_FETCH_H
