#include "PluginListHandler.h"
#include "PluginRegistry.h"
#include "IPlugin.h"

#include "httplib.h"

// windows.h (pulled in by httplib.h) defines DELETE as a macro.
#ifdef DELETE
#undef DELETE
#endif

#include <mutex>
#include <sstream>

// ---------------------------------------------------------------------------
// Auto-registration via REGISTER_HANDLER macro (runs before main)
// ---------------------------------------------------------------------------
REGISTER_HANDLER(PluginListHandler)

// ---------------------------------------------------------------------------
// Static data
// ---------------------------------------------------------------------------
std::vector<std::string> PluginListHandler::pluginNames_;
std::vector<std::string> PluginListHandler::pluginVersions_;
static std::mutex g_pluginListMutex;

// ---------------------------------------------------------------------------
PluginListHandler::PluginListHandler() = default;

const char* PluginListHandler::route()
{
    return "/api/plugins";
}

IHttpHandler::HttpMethod PluginListHandler::method()
{
    return IHttpHandler::HttpMethod::GET;
}

void PluginListHandler::handle(const char* /*request*/, void* response)
{
    auto *res = static_cast<httplib::Response *>(response);

    std::ostringstream json;
    json << "{\"plugins\":[";

    {
        std::lock_guard<std::mutex> lock(g_pluginListMutex);

        // Also include plugins from the dynamic PluginRegistry.
        auto dynamicPlugins = PluginRegistry::instance().allPlugins();

        size_t total = pluginNames_.size() + dynamicPlugins.size();
        size_t count = 0;

        // Static plugin names.
        for (size_t i = 0; i < pluginNames_.size(); ++i) {
            if (count > 0) json << ",";
            json << "{\"name\":\"" << pluginNames_[i] << "\"";
            if (i < pluginVersions_.size() && !pluginVersions_[i].empty())
                json << ",\"version\":\"" << pluginVersions_[i] << "\"";
            json << ",\"loaded\":true}";
            ++count;
        }

        // Dynamic plugins from PluginRegistry.
        for (auto *plugin : dynamicPlugins) {
            if (!plugin) continue;
            if (count > 0) json << ",";
            json << "{\"name\":\"" << plugin->name() << "\""
                 << ",\"version\":\"" << plugin->version() << "\""
                 << ",\"loaded\":true}";
            ++count;
        }
    }

    json << "]}";

    res->set_content(json.str(), "application/json");
}

void PluginListHandler::setPluginList(const std::vector<std::string> &names,
                                      const std::vector<std::string> &versions)
{
    std::lock_guard<std::mutex> lock(g_pluginListMutex);
    pluginNames_    = names;
    pluginVersions_ = versions;
}
