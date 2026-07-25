#include "StatusHandler.h"
#include "PluginRegistry.h"

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
REGISTER_HANDLER(StatusHandler)

// ---------------------------------------------------------------------------
// Static start time
// ---------------------------------------------------------------------------
std::chrono::steady_clock::time_point StatusHandler::startTime_ =
    std::chrono::steady_clock::now();

// ---------------------------------------------------------------------------
StatusHandler::StatusHandler() = default;

const char* StatusHandler::route()
{
    return "/api/status";
}

IHttpHandler::HttpMethod StatusHandler::method()
{
    return IHttpHandler::HttpMethod::GET;
}

void StatusHandler::handle(const char* /*request*/, void* response)
{
    auto *res = static_cast<httplib::Response *>(response);

    auto now = std::chrono::steady_clock::now();
    auto uptimeSec = std::chrono::duration_cast<std::chrono::seconds>(
                         now - startTime_).count();

    std::ostringstream json;
    json << "{\"status\":\"ok\",\"version\":\"0.1.0\",\"uptime\":" << uptimeSec << "}";

    res->set_content(json.str(), "application/json");
}

void StatusHandler::setStartTime(std::chrono::steady_clock::time_point t)
{
    startTime_ = t;
}

std::chrono::steady_clock::time_point StatusHandler::startTime()
{
    return startTime_;
}
