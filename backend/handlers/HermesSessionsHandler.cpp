#include "HermesSessionsHandler.h"
#include "httplib.h"
#include <QString>
#include <cstring>

HermesSessionsHandler::HermesSessionsHandler() = default;

const char* HermesSessionsHandler::route() { return "/api/hermes/sessions"; }
IHttpHandler::HttpMethod HermesSessionsHandler::method() { return HttpMethod::GET; }
void HermesSessionsHandler::setApiClient(std::shared_ptr<HermesApiClient> client) { api_ = std::move(client); }

void HermesSessionsHandler::handle(const char*, void* response) {
    auto* res = static_cast<httplib::Response*>(response);
    if (!api_) { res->set_content(R"({"error":"API not configured"})", "application/json"); res->status = 500; return; }
    res->set_content(api_->listSessions(), "application/json; charset=utf-8");
}

void HermesSessionsHandler::handleWithContext(const char*, const char* path, const char*, void* response, const char*) {
    auto* res = static_cast<httplib::Response*>(response);
    if (!api_) { res->set_content(R"({"error":"API not configured"})", "application/json"); res->status = 500; return; }
    std::string pathStr(path), prefix = "/api/hermes/sessions";
    if (pathStr.size() <= prefix.size() || pathStr == prefix) { res->set_content(api_->listSessions(), "application/json; charset=utf-8"); return; }
    std::string rest = pathStr.substr(prefix.size());
    if (!rest.empty() && rest[0] == '/') rest = rest.substr(1);
    if (rest.empty()) { res->set_content(api_->listSessions(), "application/json; charset=utf-8"); return; }
    res->set_content(api_->getMessages(rest), "application/json; charset=utf-8");
}
