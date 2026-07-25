#pragma once

#include "IHttpHandler.h"

#include <chrono>

namespace KatHub {
class SignalHub;
}

// Built-in handler: GET /api/status
// Returns JSON: {"status":"ok","version":"0.1.0","uptime":<seconds>}
class StatusHandler : public IHttpHandler
{
public:
    StatusHandler();

    const char* route() override;
    HttpMethod method() override;
    void handle(const char* request, void* response) override;

    // Set the start time reference for uptime calculation.
    static void setStartTime(std::chrono::steady_clock::time_point t);
    static std::chrono::steady_clock::time_point startTime();

    // Set the SignalHub for publishing status events.
    void setSignalHub(KatHub::SignalHub *hub);

private:
    static std::chrono::steady_clock::time_point startTime_;
    KatHub::SignalHub *signalHub_ = nullptr;
};
