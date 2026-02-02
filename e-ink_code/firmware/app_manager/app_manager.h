#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include <Arduino.h>
#include "app_interface.h"

// Forward declarations
class WiFiManager;
class DisplayManager;
class PowerManager;
class OTAManager;

class AppManager {
public:
    AppManager();
    
    // Initialize with core managers
    void setWiFiManager(WiFiManager* wifi);
    void setDisplayManager(DisplayManager* display);
    void setPowerManager(PowerManager* power);
    void setOTAManager(OTAManager* ota);
    
    // App registration
    void registerApp(AppInterface* app, const char* name);
    void setActiveApp(const char* name);
    void setActiveApp(int index);
    
    // Configuration from JSON string
    // Expected format: {"app": "fun", "config": {...}}
    bool configureFromJson(const char* jsonString);
    
    // App management
    void begin();
    void loop();
    
    // App query
    int getAppCount();
    const char* getAppName(int index);
    int getActiveAppIndex();
    const char* getActiveAppName();

private:
    static const int MAX_APPS = 10;
    AppInterface* _apps[MAX_APPS];
    const char* _appNames[MAX_APPS];
    int _appCount;
    int _activeAppIndex;
    
    WiFiManager* _wifi;
    DisplayManager* _display;
    PowerManager* _power;
    OTAManager* _ota;
};

#endif // APP_MANAGER_H
