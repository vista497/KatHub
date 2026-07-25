#pragma once

#include "IHttpHandler.h"
#include "HermesApiClient.h"

#include <memory>
#include <string>

// Proxies session requests from KatHub frontend to Hermes Agent API Server.
// Endpoints:
//   GET /api/hermes/sessions          → list all Hermes sessions
//   GET /api/hermes/sessions/{id}     → get session messages

class HermesSessionsHandler : public IHttpHandler
{
public:
    HermesSessionsHandler();

    const char* route() override;
    HttpMethod  method() override;
    void        handle(const char* request, void* response) override;
    void        handleWithContext(const char* body, const char* path,
                                  const char* query, void* response) override;

    void setApiClient(std::shared_ptr<HermesApiClient> client);

private:
    std::shared_ptr<HermesApiClient> api_;
};
