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
    static RTC_DATA_ATTR int displayMode;
    
    // Refresh interval in minutes (default: 30 minutes)
    uint32_t _refreshIntervalMinutes = 30;
    
    // Helper methods
    void handleOTA();
    void cycleDisplayMode();
};

#endif // FUN_APP_H
