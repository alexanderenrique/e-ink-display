#ifndef SHELF_APP_H
#define SHELF_APP_H

#include "../../app_manager/app_interface.h"

class ShelfApp : public AppInterface {
public:
    ShelfApp();
    virtual ~ShelfApp() {}
    
    // App lifecycle
    bool begin() override;
    void loop() override;
    void end() override;
    
    // App identification
    const char* getName() override { return "shelf"; }
    
    // Configuration (via BLE: bin ID, server URL, refresh interval)
    bool configure(const JsonObject& config) override;

private:
    // Bin ID to look up
    String _binId;
    
    // Server URL for bin lookup (e.g., "http://192.168.1.100:8080")
    String _serverUrl;
    
    // Refresh interval in minutes (default: 5)
    uint32_t _refreshIntervalMinutes = 5;
};

#endif // SHELF_APP_H
