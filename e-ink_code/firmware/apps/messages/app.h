#ifndef MESSAGES_APP_H
#define MESSAGES_APP_H

#include "../../app_manager/app_interface.h"
#include "config.h"

class MessagesApp : public AppInterface {
public:
    MessagesApp();
    virtual ~MessagesApp() {}

    bool begin() override;
    void loop() override;
    void end() override;

    const char* getName() override { return "messages"; }

    bool configure(const JsonObject& config) override;

private:
    String _messages[MESSAGES_APP_MAX_MESSAGES];
    int _messageCount = 0;
    int _currentMessageIndex = 0;
    uint32_t _refreshIntervalMinutes = MESSAGES_APP_DEFAULT_REFRESH_MINUTES;

    String buildDisplayText() const;
    void advanceToNextMessage();
};

#endif // MESSAGES_APP_H
