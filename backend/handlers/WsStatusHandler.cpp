#include "WsStatusHandler.h"
#include "PluginRegistry.h"
#include "WsServer.h"

#include "httplib.h"

// windows.h (pulled in by httplib.h) defines DELETE as a macro.
#ifdef DELETE
#undef DELETE
#endif

#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Auto-registration via REGISTER_HANDLER macro (runs before main)
// ---------------------------------------------------------------------------
REGISTER_HANDLER(WsStatusHandler)

// ---------------------------------------------------------------------------
// Static WsServer pointer
// ---------------------------------------------------------------------------
WsServer *WsStatusHandler::wsServer_ = nullptr;

// ---------------------------------------------------------------------------
WsStatusHandler::WsStatusHandler() = default;

const char* WsStatusHandler::route()
{
    return "/api/ws/status";
}

IHttpHandler::HttpMethod WsStatusHandler::method()
{
    return IHttpHandler::HttpMethod::GET;
}

void WsStatusHandler::handle(const char* /*request*/, void* response)
{
    auto *res = static_cast<httplib::Response *>(response);

    std::ostringstream json;
    json << "{\"status\":\"ok\"";

    if (wsServer_) {
        json << ",\"port\":" << wsServer_->port()
             << ",\"clients\":" << wsServer_->clientCount();

        // Build subscriptions array
        json << ",\"subscriptions\":[";
        const QStringList topics = wsServer_->subscribedTopics();
        for (int i = 0; i < topics.size(); ++i) {
            if (i > 0) json << ",";
            json << "\"" << topics[i].toStdString() << "\"";
        }
        json << "]";
    } else {
        json << ",\"port\":0"
             << ",\"clients\":0"
             << ",\"subscriptions\":[]";
    }

    json << "}";

    res->set_content(json.str(), "application/json");
}

void WsStatusHandler::setWsServer(WsServer *server)
{
    wsServer_ = server;
}
