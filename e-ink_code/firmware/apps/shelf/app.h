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
};

#endif // SHELF_APP_H
