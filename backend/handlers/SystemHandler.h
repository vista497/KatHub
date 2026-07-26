#pragma once

#include "IHttpHandler.h"
#include "HermesApiClient.h"

#include <chrono>
#include <memory>
#include <string>

// SystemHandler — aggregated system status endpoint.
// GET /api/system → JSON with health, model info, version, ports, uptime.
// Aggregates data from Hermes Agent API (health, model) + local KatHub state.

class SystemHandler : public IHttpHandler
{
public:
    SystemHandler();

    const char* route() override;
    HttpMethod  method() override;
    void        handle(const char* request, void* response) override;

    void setApiClient(std::shared_ptr<HermesApiClient> client);

    // Set server start time for uptime calculation.
    void setStartTime(std::chrono::steady_clock::time_point t);

    // Set server ports for status reporting.
    void setPorts(int httpPort, int wsPort);

private:
    std::shared_ptr<HermesApiClient> api_;
    std::chrono::steady_clock::time_point startTime_{};
    int httpPort_ = 8080;
    int wsPort_  = 8081;
};
