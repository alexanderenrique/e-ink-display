#ifndef SENSOR_APP_H
#define SENSOR_APP_H

#include "../../app_manager/app_interface.h"
#include "config.h"

class SensorApp : public AppInterface {
public:
    SensorApp();
    virtual ~SensorApp() {}

    // App lifecycle
    bool begin() override;
    void loop() override;
    void end() override;

    // App identification
    const char* getName() override { return "sensor"; }

    // Configuration (via BLE: units, wifi, nemo token/url, refresh interval, sensor ID)
    bool configure(const JsonObject& config) override;

private:
    // Display units: "C" or "F" (default F)
    String _units = "F";

    // Refresh interval in minutes (default: 1)
    uint32_t _refreshIntervalMinutes = 1;

    // Nemo API
    String _nemoToken;
    String _nemoUrl = SENSOR_APP_DEFAULT_NEMO_URL;
    String _temperatureSensorId;
    String _humiditySensorId;

    // Display: header line shown in red (e.g. "Gowning Room")
    String _sensorLocation;
};

#endif // SENSOR_APP_H
