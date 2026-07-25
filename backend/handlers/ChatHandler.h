#pragma once

#include "IHttpHandler.h"

#include <QString>
#include <QObject>

namespace KatHub {
class AIController;
class PromptManager;
}

// Built-in handler: POST /api/chat
// Accepts JSON: {"message": "user text", "agent": "default"}
// Returns JSON:  {"reply": "...", "agent": "default"}
//
// Uses AIController for AI processing, PromptManager + AgentProfile
// for system prompt injection.
class ChatHandler : public IHttpHandler
{
public:
    ChatHandler();

    const char *route() override;
    HttpMethod method() override;
    void handle(const char *request, void *response) override;

    // Inject dependencies (called during KatHubApp::init).
    void setAIController(KatHub::AIController *ctrl);
    void setPromptManager(KatHub::PromptManager *pm);

private:
    KatHub::AIController  *m_aiCtrl = nullptr;
    KatHub::PromptManager *m_promptMgr = nullptr;
};
