#pragma once

#include "IHttpHandler.h"

#include <string>

// Built-in handler: GET /api/vault/graph
// Reads Obsidian vault and returns JSON with nodes + links for D3.js force graph.
class VaultGraphHandler : public IHttpHandler
{
public:
    VaultGraphHandler();

    const char* route() override;
    HttpMethod method() override;
    void handle(const char* request, void* response) override;

    // Set the path to the Obsidian vault root.
    void setVaultPath(const std::string& path);

private:
    std::string vaultPath_;

    // Helpers
    std::string readFile(const std::string& path);
    bool isMarkdown(const std::string& path);
    void buildGraphJson(std::ostringstream& json);
};
