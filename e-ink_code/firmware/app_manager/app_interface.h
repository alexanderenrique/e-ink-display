#ifndef APP_INTERFACE_H
#define APP_INTERFACE_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Forward declarations
class WiFiManager;
class DisplayManager;
class PowerManager;
class OTAManager;

class AppInterface {
public:
    virtual ~AppInterface() {}
    
    // App lifecycle
    virtual bool begin() = 0;
    virtual void loop() = 0;
    virtual void end() = 0;
    
    // App identification
    virtual const char* getName() = 0;
    
    // Configuration (optional - apps can override if they need config)
    virtual bool configure(const JsonObject& config) { return true; }
    
    // Dependencies injection (set by AppManager)
    void setWiFiManager(WiFiManager* wifi) { _wifi = wifi; }
    void setDisplayManager(DisplayManager* display) { _display = display; }
    void setPowerManager(PowerManager* power) { _power = power; }
    void setOTAManager(OTAManager* ota) { _ota = ota; }

protected:
    WiFiManager* _wifi = nullptr;
    DisplayManager* _display = nullptr;
    PowerManager* _power = nullptr;
    OTAManager* _ota = nullptr;
};

#endif // APP_INTERFACE_H
