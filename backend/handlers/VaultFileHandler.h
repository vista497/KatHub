#pragma once

#include "IHttpHandler.h"

#include <string>

// GET /api/vault/file?path=RELATIVE_PATH
// Returns the content of a single file from the Obsidian vault.
class VaultFileHandler : public IHttpHandler
{
public:
    VaultFileHandler() = default;

    const char* route() override;
    HttpMethod method() override;
    void handle(const char* request, void* response) override;

    // Extended: uses query string for path param
    void handleWithContext(
        const char* body, const char* path, const char* query,
        void* response) override;

    void setVaultPath(const std::string& path);

private:
    std::string vaultPath_;
};
