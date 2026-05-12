#pragma once

#include <Arduino.h>

/** Nearest-point description using the static ISS landmark table (Haversine). */
String describeNearestPlace(float lat, float lon);
