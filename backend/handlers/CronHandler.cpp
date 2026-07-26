#include "CronHandler.h"
#include "HermesCliHelper.h"

#include "httplib.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>

#include <cstring>
#include <sstream>
#include <regex>

CronHandler::CronHandler() = default;

const char* CronHandler::route() { return "/api/cron"; }
IHttpHandler::HttpMethod CronHandler::method() { return HttpMethod::ALL; }

static std::string loadCronList()
{
    std::string raw = HermesCliHelper::run("cron list");
    if (raw.empty()) return "[]";

    // Parse table: each job starts with "  <id> [active|paused]"
    QJsonArray arr;
    std::istringstream stream(raw);
    std::string line;
    QJsonObject job;

    std::regex idRe(R"(^\s+([a-f0-9]{8,})\s+\[(\w+)\])");
    std::regex nameRe(R"(^\s+Name:\s+(.+))");
    std::regex schedRe(R"(^\s+Schedule:\s+(.+))");
    std::regex lastRunRe(R"(^\s+Last run:\s+(.+))");
    std::regex nextRunRe(R"(^\s+Next run:\s+(.+))");

    while (std::getline(stream, line)) {
        std::smatch m;
        if (std::regex_search(line, m, idRe)) {
            if (!job.isEmpty()) arr.append(job);
            job = QJsonObject();
            job["id"] = QString::fromStdString(m[1].str());
            job["enabled"] = (m[2].str() == "active");
        } else if (std::regex_search(line, m, nameRe)) {
            job["name"] = QString::fromStdString(m[1].str());
        } else if (std::regex_search(line, m, schedRe)) {
            job["schedule"] = QString::fromStdString(m[1].str());
        } else if (std::regex_search(line, m, lastRunRe)) {
            job["last_run"] = QString::fromStdString(m[1].str());
        } else if (std::regex_search(line, m, nextRunRe)) {
            job["next_run"] = QString::fromStdString(m[1].str());
        }
    }
    if (!job.isEmpty()) arr.append(job);

    return QJsonDocument(arr).toJson(QJsonDocument::Compact).toStdString();
}

void CronHandler::handle(const char*, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);
    res->set_content(loadCronList(), "application/json; charset=utf-8");
}

void CronHandler::handleWithContext(
    const char* /*body*/, const char* path, const char* /*query*/,
    void* response, const char* /*method*/)
{
    auto* res = static_cast<httplib::Response*>(response);
    // All methods return the cron list for now
    res->set_content(loadCronList(), "application/json; charset=utf-8");
}
