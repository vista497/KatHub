#pragma once

#include "IHttpHandler.h"

#include <memory>
#include <string>

class HermesApiClient;

// Chat handler — proxies messages to Hermes Agent API Server.
// Accepts: POST /api/chat  {"message":"...", "sessionId":"..."}
// Returns: JSON with "reply" and "sessionId" fields.

class ChatHandler : public IHttpHandler
{
public:
    ChatHandler();

    const char* route() override;
    HttpMethod  method() override;
    void        handle(const char* request, void* response) override;

    void setApiClient(std::shared_ptr<HermesApiClient> client);

private:
    std::shared_ptr<HermesApiClient> api_;
};
