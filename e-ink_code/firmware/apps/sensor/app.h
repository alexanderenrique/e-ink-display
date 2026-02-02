#ifndef SENSOR_APP_H
#define SENSOR_APP_H

#include "../../app_manager/app_interface.h"

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
};

#endif // SENSOR_APP_H
