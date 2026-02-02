#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include <esp_sleep.h>
#include "hardware_config.h"

class PowerManager {
public:
    PowerManager();
    
    // Voltage reading
    int getBatteryPercentage();
    float getBatteryVoltage();
    
    // Sleep management
    void enterDeepSleep(uint64_t sleepTimeSeconds);
    esp_sleep_wakeup_cause_t getWakeupCause();
    
    // Peripheral management
    void disablePeripherals();
};

#endif // POWER_MANAGER_H
