#pragma once

#include "IHttpHandler.h"

#include <memory>
#include <string>

class HermesApiClient;

// Serves GET /api/hermes/sessions and GET /api/hermes/sessions/<id>.
// Primary path: Hermes API server (structured JSON). Falls back to the
// `hermes` CLI (sessions list / sessions export) when api_server is down.
class HermesSessionsHandler : public IHttpHandler
{
public:
    HermesSessionsHandler();
    const char* route() override;
    HttpMethod method() override;
    void handle(const char* request, void* response) override;
    void handleWithContext(const char* body, const char* path,
                          const char* query, void* response,
                          const char* method = nullptr) override;
    void setApiClient(std::shared_ptr<HermesApiClient> client);

private:
    std::shared_ptr<HermesApiClient> api_;
};
