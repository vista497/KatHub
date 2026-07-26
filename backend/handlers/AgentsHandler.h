#pragma once

#include "IHttpHandler.h"
#include "HermesApiClient.h"

#include <memory>
#include <string>

// AgentsHandler — agent profile management endpoints.
// Endpoints:
//   GET  /api/agents              → list all profiles with status
//   GET  /api/agents/{name}       → get single profile status
//   POST /api/agents/{name}/toggle → start/stop agent (registered via httplib directly)
//
// The POST /api/agents/{name}/toggle route is registered in KatHubApp::init()
// via httpServer_->server().Post() because IHttpHandler supports only one method.

class AgentsHandler : public IHttpHandler
{
public:
    AgentsHandler();

    const char* route() override;
    HttpMethod  method() override;
    void        handle(const char* request, void* response) override;
    void        handleWithContext(const char* body, const char* path,
                                  const char* query, void* response,
                                  const char* method = nullptr) override;

    void setApiClient(std::shared_ptr<HermesApiClient> client);

private:
    std::shared_ptr<HermesApiClient> api_;
};
