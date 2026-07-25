#include "HermesSessionsHandler.h"

#include "httplib.h"

#include <cstring>

HermesSessionsHandler::HermesSessionsHandler() = default;

const char* HermesSessionsHandler::route()
{
    return "/api/hermes/sessions";
}

IHttpHandler::HttpMethod HermesSessionsHandler::method()
{
    return HttpMethod::GET;
}

void HermesSessionsHandler::setApiClient(std::shared_ptr<HermesApiClient> client)
{
    api_ = std::move(client);
}

void HermesSessionsHandler::handle(const char*, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);
    if (!api_) {
        res->set_content(R"({"error":"Hermes API client not configured"})", "application/json");
        res->status = 500;
        return;
    }
    res->set_content(api_->listSessions(), "application/json; charset=utf-8");
}

void HermesSessionsHandler::handleWithContext(
    const char* /*body*/, const char* path, const char* /*query*/, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);
    if (!api_) {
        res->set_content(R"({"error":"Hermes API client not configured"})", "application/json");
        res->status = 500;
        return;
    }

    // Path format: /api/hermes/sessions[/{id}[/messages]]
    std::string pathStr(path);
    std::string prefix = "/api/hermes/sessions";

    if (pathStr.size() <= prefix.size() || pathStr == prefix) {
        res->set_content(api_->listSessions(), "application/json; charset=utf-8");
        return;
    }

    // Extract the rest: /{id}[/messages]
    std::string rest = pathStr.substr(prefix.size());
    if (!rest.empty() && rest[0] == '/') rest = rest.substr(1);

    const std::string msgSuffix = "/messages";
    std::string sessionId;
    bool wantMessages = false;

    if (rest.size() > msgSuffix.size() &&
        rest.compare(rest.size() - msgSuffix.size(), msgSuffix.size(), msgSuffix) == 0) {
        sessionId = rest.substr(0, rest.size() - msgSuffix.size());
        wantMessages = true;
    } else {
        sessionId = rest;
        wantMessages = true;
    }

    if (sessionId.empty()) {
        res->set_content(api_->listSessions(), "application/json; charset=utf-8");
        return;
    }

    res->set_content(api_->getMessages(sessionId), "application/json; charset=utf-8");
}
