#pragma once

#include "IHttpHandler.h"

#include <mutex>
#include <string>
#include <vector>

// Chat with a specific Hermes agent profile (analyst, writer, marketer, coder,
// orchestrator). Each request shells out to `hermes -p <agent> chat -q -Q`
// (new session) or `-r <sessionId>` (resume), so every agent keeps its own
// session history in Hermes.
//
// POST /api/agent-chat  {agent, message, sessionId?}
//   → {reply, sessionId, error?}
//
// GET /api/agent-chat   → {agents: [{name, label, description}...]}
class AgentChatHandler : public IHttpHandler
{
public:
    AgentChatHandler();
    const char* route() override;
    HttpMethod  method() override;
    void        handle(const char* request, void* response) override;
    void        handleWithContext(const char* body, const char* path,
                                  const char* query, void* response,
                                  const char* method) override;

private:
    void handleChat(const char* body, void* response);
    void handleList(void* response);

    std::mutex mutex_;
};
