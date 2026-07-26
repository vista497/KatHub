#include "AgentsHandler.h"
#include "HermesCliHelper.h"

#include "httplib.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>

#include <sstream>
#include <regex>

AgentsHandler::AgentsHandler() = default;

const char* AgentsHandler::route() { return "/api/agents"; }
IHttpHandler::HttpMethod AgentsHandler::method() { return HttpMethod::ALL; }

static std::string loadAgentsList()
{
    std::string raw = HermesCliHelper::run("profile list");
    if (raw.empty()) return "[]";

    // Parse table: Profile | Model | Gateway | Alias | Distribution
    // Skip header/separator lines
    QJsonArray arr;
    std::istringstream stream(raw);
    std::string line;

    // Data lines have "  profilename  " pattern
    std::regex dataRe(R"(^\s+(\S+)\s+(.+?)\s{2,}(\S+)\s+(.+?)\s{2,}(.+))");
    std::regex headerRe(R"(Profile\s+Model)");

    while (std::getline(stream, line)) {
        // Skip headers and separators
        if (std::regex_search(line, headerRe)) continue;
        if (line.find("─") != std::string::npos) continue;
        if (line.empty()) continue;

        std::smatch m;
        if (std::regex_search(line, m, dataRe)) {
            QJsonObject agent;
            QString name = QString::fromStdString(m[1].str()).trimmed();
            if (name.startsWith("◆")) name = name.mid(1); // remove active marker
            agent["name"] = name;
            agent["model"] = QString::fromStdString(m[2].str()).trimmed();
            agent["status"] = QString::fromStdString(m[3].str()).trimmed();
            agent["active"] = (line.find("◆") != std::string::npos);
            arr.append(agent);
        }
    }
    return QJsonDocument(arr).toJson(QJsonDocument::Compact).toStdString();
}

void AgentsHandler::handle(const char*, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);
    res->set_content(loadAgentsList(), "application/json; charset=utf-8");
}

void AgentsHandler::handleWithContext(
    const char*, const char*, const char*, void* response, const char*)
{
    auto* res = static_cast<httplib::Response*>(response);
    res->set_content(loadAgentsList(), "application/json; charset=utf-8");
}
