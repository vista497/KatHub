#pragma once

#include "IHttpHandler.h"

#include <string>

// GET /api/vault/sessions — list all sessions
// GET /api/vault/sessions?session=NAME — load messages from a session
class VaultSessionsHandler : public IHttpHandler
{
public:
    VaultSessionsHandler() = default;

    const char* route() override;
    HttpMethod method() override;
    void handle(const char* request, void* response) override;

    void handleWithContext(
        const char* body, const char* path, const char* query,
        void* response) override;

    void setVaultPath(const std::string& path);

private:
    std::string vaultPath_;
};
