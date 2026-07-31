#pragma once

#include "IHttpHandler.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class HermesApiClient;

class ChatHandler : public IHttpHandler
{
public:
    ChatHandler();
    const char* route() override;
    HttpMethod  method() override;
    void        handle(const char* request, void* response) override;
    void        handleWithContext(const char* body, const char* path,
                                  const char* query, void* response,
                                  const char* method) override;
    void setApiClient(std::shared_ptr<HermesApiClient> client);

    // Hermes profile used for chat runs (`hermes -p <profile> -z ...`).
    // Defaults to "default"; wired from HERMES_PROFILE in KatHubApp.
    void setProfile(const std::string& profile);

    // A pending approval surfaced from a running /v1/runs stream.
    struct PendingApproval
    {
        std::string runId;
        std::string sessionId;
        std::string command;
        std::string description;
        std::vector<std::string> choices;
        double timestamp = 0.0;
    };

private:
    void handleChat(const char* request, void* response);
    void handleApprovals(const char* body, const char* query,
                         void* response, const char* method);

    std::shared_ptr<HermesApiClient> api_;
    std::string profile_ = "default";
    std::mutex approvalsMutex_;
    // sessionId -> latest pending approval (most recent run wins)
    std::map<std::string, PendingApproval> pendingBySession_;
    // runId -> sessionId, for cleanup on run end
    std::map<std::string, std::string> sessionByRun_;
};
