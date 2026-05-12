#include "iss_geo_lookup.h"
#include <Arduino.h>
#include <cmath>

namespace {

constexpr float kEarthRadiusKm = 6371.0f;
constexpr float kDegToRad = 0.017453292519943295f;
/** Beyond this distance, show "~mi" so the label is not read as "directly overhead". */
constexpr float kNearThresholdKm = 600.0f;

static float haversineKm(float lat1, float lon1, float lat2, float lon2) {
    float dlat = (lat2 - lat1) * kDegToRad;
    float dlon = (lon2 - lon1) * kDegToRad;
    float a1 = lat1 * kDegToRad;
    float a2 = lat2 * kDegToRad;
    float h = sinf(dlat * 0.5f) * sinf(dlat * 0.5f) +
              cosf(a1) * cosf(a2) * sinf(dlon * 0.5f) * sinf(dlon * 0.5f);
    if (h >= 1.0f) h = 1.0f;
    float c = 2.0f * atan2f(sqrtf(h), sqrtf(1.0f - h));
    return kEarthRadiusKm * c;
}

struct GeoPoint {
    float lat;
    float lon;
    const char* label;
};

// Approximate centers; ASCII-only labels. Mix of major cities and sparse regional anchors.
static const GeoPoint kPoints[] = {
    // North America
    {40.71f, -74.01f, "New York, USA"},
    {34.05f, -118.24f, "Los Angeles, USA"},
    {41.88f, -87.63f, "Chicago, USA"},
    {29.76f, -95.37f, "Houston, USA"},
    {25.76f, -80.19f, "Miami, USA"},
    {47.61f, -122.33f, "Seattle, USA"},
    {33.45f, -112.07f, "Phoenix, USA"},
    {39.74f, -104.99f, "Denver, USA"},
    {32.78f, -96.80f, "Dallas, USA"},
    {33.75f, -84.39f, "Atlanta, USA"},
    {42.36f, -71.06f, "Boston, USA"},
    {37.77f, -122.42f, "San Francisco, USA"},
    {36.17f, -115.14f, "Las Vegas, USA"},
    {45.52f, -122.68f, "Portland, USA"},
    {40.76f, -111.89f, "Salt Lake City, USA"},
    {44.98f, -93.27f, "Minneapolis, USA"},
    {42.33f, -83.05f, "Detroit, USA"},
    {39.95f, -75.17f, "Philadelphia, USA"},
    {38.63f, -90.20f, "St Louis, USA"},
    {39.10f, -94.58f, "Kansas City, USA"},
    {35.23f, -80.84f, "Charlotte, USA"},
    {61.22f, -149.90f, "Anchorage, USA"},
    {21.31f, -157.86f, "Honolulu, USA"},
    {19.43f, -99.13f, "Mexico City, Mexico"},
    {14.63f, -90.51f, "Guatemala City, Guatemala"},
    {43.65f, -79.38f, "Toronto, Canada"},
    {49.28f, -123.12f, "Vancouver, Canada"},
    {45.50f, -73.57f, "Montreal, Canada"},
    {51.04f, -114.07f, "Calgary, Canada"},
    {53.55f, -113.49f, "Edmonton, Canada"},
    {23.13f, -82.37f, "Havana, Cuba"},
    {8.98f, -79.52f, "Panama City, Panama"},
    // South America
    {4.71f, -74.07f, "Bogota, Colombia"},
    {6.25f, -75.56f, "Medellin, Colombia"},
    {-12.05f, -77.04f, "Lima, Peru"},
    {-0.18f, -78.47f, "Quito, Ecuador"},
    {-33.45f, -70.67f, "Santiago, Chile"},
    {-16.50f, -68.15f, "La Paz, Bolivia"},
    {-34.60f, -58.38f, "Buenos Aires, Argentina"},
    {-34.90f, -56.17f, "Montevideo, Uruguay"},
    {-23.55f, -46.63f, "Sao Paulo, Brazil"},
    {-22.91f, -43.17f, "Rio de Janeiro, Brazil"},
    {10.48f, -66.90f, "Caracas, Venezuela"},
    // Europe
    {51.51f, -0.13f, "London, UK"},
    {53.48f, -2.24f, "Manchester, UK"},
    {48.86f, 2.35f, "Paris, France"},
    {40.42f, -3.70f, "Madrid, Spain"},
    {41.39f, 2.15f, "Barcelona, Spain"},
    {52.52f, 13.41f, "Berlin, Germany"},
    {48.14f, 11.58f, "Munich, Germany"},
    {52.37f, 4.90f, "Amsterdam, Netherlands"},
    {55.68f, 12.57f, "Copenhagen, Denmark"},
    {41.90f, 12.48f, "Rome, Italy"},
    {45.46f, 9.19f, "Milan, Italy"},
    {48.21f, 16.37f, "Vienna, Austria"},
    {52.23f, 21.01f, "Warsaw, Poland"},
    {50.08f, 14.44f, "Prague, Czechia"},
    {47.50f, 19.04f, "Budapest, Hungary"},
    {37.98f, 23.73f, "Athens, Greece"},
    {44.43f, 26.10f, "Bucharest, Romania"},
    {38.72f, -9.14f, "Lisbon, Portugal"},
    {53.35f, -6.26f, "Dublin, Ireland"},
    {59.33f, 18.07f, "Stockholm, Sweden"},
    {59.91f, 10.75f, "Oslo, Norway"},
    {60.17f, 24.94f, "Helsinki, Finland"},
    {64.15f, -21.95f, "Reykjavik, Iceland"},
    {50.45f, 30.52f, "Kyiv, Ukraine"},
    {55.76f, 37.62f, "Moscow, Russia"},
    {59.93f, 30.34f, "St Petersburg, Russia"},
    {55.01f, 82.94f, "Novosibirsk, Russia"},
    {43.12f, 131.90f, "Vladivostok, Russia"},
    {55.83f, 49.12f, "Kazan, Russia"},
    {68.97f, 33.10f, "Murmansk, Russia"},
    {62.03f, 129.73f, "Yakutsk, Russia"},
    {41.01f, 28.98f, "Istanbul, Turkey"},
    // Africa & Middle East
    {30.04f, 31.24f, "Cairo, Egypt"},
    {6.52f, 3.38f, "Lagos, Nigeria"},
    {15.50f, 32.56f, "Khartoum, Sudan"},
    {-1.29f, 36.82f, "Nairobi, Kenya"},
    {-6.79f, 39.28f, "Dar es Salaam, Tanzania"},
    {-26.20f, 28.04f, "Johannesburg, South Africa"},
    {-33.93f, 18.42f, "Cape Town, South Africa"},
    {33.57f, -7.59f, "Casablanca, Morocco"},
    {9.03f, 38.75f, "Addis Ababa, Ethiopia"},
    {-4.32f, 15.31f, "Kinshasa, DR Congo"},
    {14.69f, -17.45f, "Dakar, Senegal"},
    {-18.88f, 47.51f, "Antananarivo, Madagascar"},
    {25.20f, 55.27f, "Dubai, UAE"},
    {24.71f, 46.68f, "Riyadh, Saudi Arabia"},
    {32.09f, 34.78f, "Tel Aviv, Israel"},
    {35.69f, 51.39f, "Tehran, Iran"},
    {33.31f, 44.37f, "Baghdad, Iraq"},
    // South & Central Asia
    {24.86f, 67.00f, "Karachi, Pakistan"},
    {23.81f, 90.41f, "Dhaka, Bangladesh"},
    {28.61f, 77.21f, "Delhi, India"},
    {19.08f, 72.88f, "Mumbai, India"},
    {22.57f, 88.36f, "Kolkata, India"},
    {13.08f, 80.27f, "Chennai, India"},
    {6.93f, 79.85f, "Colombo, Sri Lanka"},
    {51.13f, 71.43f, "Astana, Kazakhstan"},
    {47.92f, 106.92f, "Ulaanbaatar, Mongolia"},
    // East & Southeast Asia, Oceania
    {35.68f, 139.76f, "Tokyo, Japan"},
    {37.57f, 126.98f, "Seoul, South Korea"},
    {39.90f, 116.41f, "Beijing, China"},
    {31.23f, 121.47f, "Shanghai, China"},
    {22.32f, 114.17f, "Hong Kong, China"},
    {25.03f, 121.56f, "Taipei, Taiwan"},
    {14.60f, 120.98f, "Manila, Philippines"},
    {13.76f, 100.50f, "Bangkok, Thailand"},
    {1.35f, 103.82f, "Singapore"},
    {-6.21f, 106.85f, "Jakarta, Indonesia"},
    {3.14f, 101.69f, "Kuala Lumpur, Malaysia"},
    {21.03f, 105.85f, "Hanoi, Vietnam"},
    {16.87f, 96.20f, "Yangon, Myanmar"},
    {11.56f, 104.93f, "Phnom Penh, Cambodia"},
    {4.90f, 114.94f, "Bandar Seri Begawan, Brunei"},
    {-8.56f, 125.58f, "Dili, Timor-Leste"},
    {-9.44f, 147.18f, "Port Moresby, Papua New Guinea"},
    {-33.87f, 151.21f, "Sydney, Australia"},
    {-37.81f, 144.96f, "Melbourne, Australia"},
    {-27.47f, 153.03f, "Brisbane, Australia"},
    {-31.95f, 115.86f, "Perth, Australia"},
    {-34.93f, 138.60f, "Adelaide, Australia"},
    {-12.46f, 130.84f, "Darwin, Australia"},
    {-23.70f, 133.87f, "Alice Springs, Australia"},
    {-36.85f, 174.76f, "Auckland, New Zealand"},
    {-43.53f, 172.64f, "Christchurch, New Zealand"},
    // Islands & polar / sparse anchors
    {64.18f, -51.72f, "Nuuk, Greenland"},
    {37.74f, -25.67f, "Ponta Delgada, Azores"},
    {-18.14f, 178.44f, "Suva, Fiji"},
    {-17.54f, -149.57f, "Papeete, French Polynesia"},
    {13.47f, 144.75f, "Hagatna, Guam"},
    {-22.28f, 166.45f, "Noumea, New Caledonia"},
    {-27.11f, -109.35f, "Easter Island, Chile"},
};

}  // namespace

String describeNearestPlace(float lat, float lon) {
    const size_t n = sizeof(kPoints) / sizeof(kPoints[0]);
    float bestKm = 1.0e12f;
    const char* bestLabel = "unknown area";

    for (size_t i = 0; i < n; ++i) {
        float d = haversineKm(lat, lon, kPoints[i].lat, kPoints[i].lon);
        if (d < bestKm) {
            bestKm = d;
            bestLabel = kPoints[i].label;
        }
    }

    float distMi = bestKm * 0.621371f;
    if (bestKm < kNearThresholdKm) {
        return String("Roughly near: ") + bestLabel;
    }
    return String("Closest listed: ") + bestLabel + " (~" + String(distMi, 0) + " mi)";
}
