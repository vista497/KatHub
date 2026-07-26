#include "SkillsHandler.h"
#include "HermesCliHelper.h"

#include "httplib.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>

#include <sstream>
#include <regex>

SkillsHandler::SkillsHandler() = default;

const char* SkillsHandler::route() { return "/api/skills"; }
IHttpHandler::HttpMethod SkillsHandler::method() { return HttpMethod::ALL; }

static std::string loadSkillsList()
{
    std::string raw = HermesCliHelper::run("skills list");
    if (raw.empty()) return "[]";

    QJsonArray arr;
    std::istringstream stream(raw);
    std::string line;
    
    // Table format: │ name │ category │ source │ trust │ status │
    std::regex dataRe(R"(^\│\s+(.+?)\s+│\s+(.+?)\s+│\s+(.+?)\s+│\s+(.+?)\s+│\s+(.+?)\s+│)");
    bool inTable = false;
    bool headerSkipped = false;

    while (std::getline(stream, line)) {
        // Detect table separators (lines with ─)
        if (line.find("─") != std::string::npos) {
            if (!inTable) { inTable = true; continue; }
        }
        if (!inTable) continue;
        
        // Skip the header row (first row with column names)
        if (!headerSkipped) {
            headerSkipped = true;
            continue;
        }

        std::smatch m;
        if (std::regex_search(line, m, dataRe)) {
            QJsonObject skill;
            auto trim = [](const std::string& s) -> QString {
                QString q = QString::fromStdString(s).trimmed();
                // Remove trailing "..." from truncated names
                if (q.endsWith("…")) q.chop(1);
                return q;
            };
            skill["name"] = trim(m[1].str());
            skill["category"] = trim(m[2].str());
            skill["source"] = trim(m[3].str());
            skill["status"] = trim(m[5].str());
            arr.append(skill);
        }
    }
    return QJsonDocument(arr).toJson(QJsonDocument::Compact).toStdString();
}

void SkillsHandler::handle(const char*, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);
    res->set_content(loadSkillsList(), "application/json; charset=utf-8");
}

void SkillsHandler::handleWithContext(
    const char*, const char*, const char*, void* response, const char*)
{
    auto* res = static_cast<httplib::Response*>(response);
    res->set_content(loadSkillsList(), "application/json; charset=utf-8");
}
