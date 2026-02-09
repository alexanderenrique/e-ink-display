#ifndef FUN_APP_H
#define FUN_APP_H

#include "../../app_manager/app_interface.h"

class FunApp : public AppInterface {
public:
    FunApp();
    virtual ~FunApp() {}
    
    // App lifecycle
    bool begin() override;
    void loop() override;
    void end() override;
    
    // App identification
    const char* getName() override { return "fun"; }
    
    // Configuration
    bool configure(const JsonObject& config) override;

private:
    // Display mode tracking (persists across deep sleep)
    // 0=room_data, 1=earthquake, 2=cat_facts, 3=iss, 4=useless_facts
    static RTC_DATA_ATTR int displayMode;
    
    // Refresh interval in minutes (default: 2 minutes)
    uint32_t _refreshIntervalMinutes = 2;
    
    // API enable flags from config (which data sources to show)
    bool _apiRoomData = true;
    bool _apiCatFacts = true;
    bool _apiEarthquake = true;
    bool _apiISS = true;
    bool _apiUselessFacts = true;
    
    // Helper methods
    void handleOTA();
    void cycleDisplayMode();
    bool isModeEnabled(int mode) const;
};

#endif // FUN_APP_H
