#include "power_manager.h"
#include "hardware_config.h"

PowerManager::PowerManager() {
}

int PowerManager::getBatteryPercentage() {
    // Set pin as input and configure attenuation
    pinMode(V_ADC, INPUT);
    analogSetPinAttenuation(V_ADC, ADC_ATTENDB_MAX);
    pinMode(V_SWITCH, OUTPUT);
    digitalWrite(V_SWITCH, LOW);
    delay(100);

    float voltageMilliVoltsSum = 0;
    for (int i = 0; i < 10; i++) {
        voltageMilliVoltsSum += analogReadMilliVolts(V_ADC);
        delay(10);
    }
    float voltageMilliVolts = voltageMilliVoltsSum / 10.0;
    float voltageAtADC = voltageMilliVolts / 1000.0;

    pinMode(V_SWITCH, INPUT);
    delay(100);

    float scalingFactor = (float)BATTERY_R2 / (BATTERY_R1 + BATTERY_R2);
    float batteryVoltage = voltageAtADC / scalingFactor;

    float voltageRange = BATTERY_HIGH_VOLTAGE - BATTERY_LOW_VOLTAGE;
    float voltagePercentage = ((batteryVoltage - BATTERY_LOW_VOLTAGE) / voltageRange) * 100.0;
    
    // Clamp percentage between 0% and 100%
    if (voltagePercentage < 0.0) voltagePercentage = 0.0;
    if (voltagePercentage > 100.0) voltagePercentage = 100.0;

    return (int)(voltagePercentage + 0.5f);
}

float PowerManager::getBatteryVoltage() {
    // Similar to getBatteryPercentage but returns voltage instead
    pinMode(V_ADC, INPUT);
    analogSetPinAttenuation(V_ADC, ADC_ATTENDB_MAX);
    pinMode(V_SWITCH, OUTPUT);
    digitalWrite(V_SWITCH, LOW);
    delay(100);

    float voltageMilliVoltsSum = 0;
    for (int i = 0; i < 10; i++) {
        voltageMilliVoltsSum += analogReadMilliVolts(V_ADC);
        delay(10);
    }
    float voltageMilliVolts = voltageMilliVoltsSum / 10.0;
    float voltageAtADC = voltageMilliVolts / 1000.0;

    pinMode(V_SWITCH, INPUT);
    delay(100);

    float scalingFactor = (float)BATTERY_R2 / (BATTERY_R1 + BATTERY_R2);
    return voltageAtADC / scalingFactor;
}

void PowerManager::enterDeepSleep(uint64_t sleepTimeSeconds) {
    Serial.print("Entering deep sleep for ");
    Serial.print(sleepTimeSeconds);
    Serial.println(" seconds...");
    
    // Disable all peripherals before sleep
    disablePeripherals();
    
    // Flush serial output before sleep
    Serial.flush();
    delay(100);
    
    // Configure deep sleep timer
    esp_sleep_enable_timer_wakeup(sleepTimeSeconds * 1000000ULL); // Convert to microseconds
    
    // Enter deep sleep
    esp_deep_sleep_start();
    // Code never reaches here - device will restart after wake
}

void PowerManager::enterLowBatterySleep() {
    Serial.println("Entering low battery sleep mode (periodic wakeup to check battery)...");
    
    // Disable all peripherals before sleep
    disablePeripherals();
    
    // Flush serial output before sleep
    Serial.flush();
    delay(100);
    
    // Configure deep sleep timer for periodic wakeup
    // Wake up every LOW_BATTERY_WAKEUP_INTERVAL_SECONDS to check battery
    esp_sleep_enable_timer_wakeup(LOW_BATTERY_WAKEUP_INTERVAL_SECONDS * 1000000ULL); // Convert to microseconds
    
    // Enter deep sleep
    esp_deep_sleep_start();
    // Code never reaches here - device will restart after wake
}

esp_sleep_wakeup_cause_t PowerManager::getWakeupCause() {
    return esp_sleep_get_wakeup_cause();
}

void PowerManager::disablePeripherals() {
    Serial.println("Disabling peripherals for sleep...");
    
    // Note: WiFi should be disabled by WiFiManager before calling this
    // SPI and I2C should be disabled by their respective managers
    
    // Set SPI pins to high impedance/low power state to reduce leakage
    // These pin numbers should match your hardware configuration
    // Note: These are defined in main.h, but we'll need to pass them or define them here
    // For now, we'll leave this as a placeholder that apps can call after disabling their peripherals
    
    Serial.println("All peripherals disabled");
}
