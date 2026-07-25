#pragma once

#include "IHttpHandler.h"

#include <vector>
#include <string>

/// Built-in handler: GET /api/plugins
/// Returns JSON array of loaded plugin metadata:
///   {"plugins": [{"name": "...", "version": "...", "loaded": true}, ...]}
class PluginListHandler : public IHttpHandler
{
public:
    PluginListHandler();

    const char* route() override;
    HttpMethod method() override;
    void handle(const char* request, void* response) override;

    /// Set the list of plugin names to report.
    /// Thread-safe — copies the vector.
    static void setPluginList(const std::vector<std::string> &names,
                              const std::vector<std::string> &versions);

private:
    static std::vector<std::string> pluginNames_;
    static std::vector<std::string> pluginVersions_;
};
