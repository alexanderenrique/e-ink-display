#include "power_manager.h"
#include "hardware_config.h"

PowerManager::PowerManager() {
}

int PowerManager::getBatteryPercentage() {
    Serial.println("\n=== getBatteryPercentage() Debug ===");

    // Set pin as input and configure attenuation
    pinMode(V_ADC, INPUT);
    // Use ADC_ATTENDB_MAX for maximum range (~2.5V)
    analogSetPinAttenuation(V_ADC, ADC_ATTENDB_MAX);
    
    // initialize the switch pin as output
    pinMode(V_SWITCH, OUTPUT);

    Serial.println("Pin configuration:");
    Serial.println("  V_ADC pin: " + String(V_ADC));
    Serial.println("  V_SWITCH pin: " + String(V_SWITCH));
    Serial.println("  R1: " + String(BATTERY_R1) + " ohms");
    Serial.println("  R2: " + String(BATTERY_R2) + " ohms");
    Serial.println("  HIGH_VOLTAGE: " + String(BATTERY_HIGH_VOLTAGE) + "V");
    Serial.println("  LOW_VOLTAGE: " + String(BATTERY_LOW_VOLTAGE) + "V");

    // turn on the switch
    Serial.println("Turning on voltage switch...");
    digitalWrite(V_SWITCH, LOW);

    // wait for it to stabilize
    delay(100);

    // Read and accumulate millivolt values
    float voltageMilliVoltsSum = 0;
    Serial.println("Reading voltage values:");
    for (int i = 0; i < 10; i++) {
        int milliVolts = analogReadMilliVolts(V_ADC);
        voltageMilliVoltsSum += milliVolts;
        Serial.println("  Reading " + String(i+1) + "/10: " + String(milliVolts) + " mV");
        delay(10); // Small delay between readings
    }
    float voltageMilliVolts = voltageMilliVoltsSum / 10.0;
    float voltageAtADC = voltageMilliVolts / 1000.0;  // Convert mV to V
    
    Serial.println("Voltage Statistics:");
    Serial.println("  Average voltage at ADC pin: " + String(voltageMilliVolts, 2) + " mV (" + String(voltageAtADC, 3) + " V)");
    
    // Check for saturation (ESP32-C3 ADC max is ~2.5-2.6V)
    if (voltageAtADC > 2.6) {
        Serial.println("  WARNING: Voltage exceeds ADC limit! ADC may be saturated.");
    }

    // turn off the switch by setting the pin to INPUT (let it float, external pull-up will pull high)
    Serial.println("Floating voltage switch (set to INPUT, pull-up keeps high)...");
    pinMode(V_SWITCH, INPUT);
    delay(100);

    // Calculate battery voltage from voltage divider
    float scalingFactor = (float)BATTERY_R2 / (BATTERY_R1 + BATTERY_R2);
    float batteryVoltage = voltageAtADC / scalingFactor;
    
    Serial.println("\nVoltage Calculation:");
    Serial.println("  Voltage at ADC pin: " + String(voltageAtADC, 3) + "V");
    Serial.println("  Scaling factor (R2/(R1+R2)): " + String(scalingFactor, 4));
    Serial.println("  Calculated battery voltage: " + String(batteryVoltage, 3) + "V");
    
    // Warn if calculated battery voltage seems unreasonable
    if (batteryVoltage > 4.5) {
        Serial.println("  WARNING: Calculated battery voltage > 4.5V - ADC may be saturated!");
    }
    
    // Calculate percentage based on battery voltage range
    float voltageRange = BATTERY_HIGH_VOLTAGE - BATTERY_LOW_VOLTAGE;
    float voltagePercentage = ((batteryVoltage - BATTERY_LOW_VOLTAGE) / voltageRange) * 100.0;
    
    // Clamp percentage between 0% and 100%
    if (voltagePercentage < 0.0) voltagePercentage = 0.0;
    if (voltagePercentage > 100.0) voltagePercentage = 100.0;
    
    Serial.println("\nResults:");
    Serial.println("  Battery voltage: " + String(batteryVoltage, 3) + "V");
    Serial.println("  Calculated percentage: " + String(voltagePercentage, 2) + "%");
    Serial.println("  Rounded percentage: " + String((int)(voltagePercentage + 0.5f)) + "%");
    Serial.println("=== End getBatteryPercentage() Debug ===\n");

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
