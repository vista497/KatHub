#include "KanbanHandler.h"
#include "HermesCliHelper.h"

#include "httplib.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>

#include <sstream>
#include <regex>

KanbanHandler::KanbanHandler() = default;

const char* KanbanHandler::route() { return "/api/kanban"; }
IHttpHandler::HttpMethod KanbanHandler::method() { return HttpMethod::ALL; }

static std::string loadKanbanList()
{
    std::string raw = HermesCliHelper::run("kanban list");
    if (raw.empty()) return "[]";

    QJsonArray arr;
    std::istringstream stream(raw);
    std::string line;

    // Parse lines like: "⊘ t_215ed1be  blocked   default    Title..."
    // Skip the board header line ("Board: ...")
    std::regex taskRe(R"(^(\S+)\s+(t_\S+)\s+(\S+)\s+(\S+)\s+(.+))");

    while (std::getline(stream, line)) {
        // Skip board header
        if (line.find("Board:") != std::string::npos) continue;
        if (line.empty()) continue;

        std::smatch m;
        if (std::regex_search(line, m, taskRe)) {
            QJsonObject task;
            std::string symbol = m[1].str();
            std::string id = m[2].str();
            std::string status = m[3].str();
            
            task["id"] = QString::fromStdString(id);
            task["status"] = QString::fromStdString(status);
            task["assignee"] = QString::fromStdString(m[4].str());
            task["title"] = QString::fromStdString(m[5].str());
            arr.append(task);
        }
    }
    return QJsonDocument(arr).toJson(QJsonDocument::Compact).toStdString();
}

void KanbanHandler::handle(const char*, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);
    res->set_content(loadKanbanList(), "application/json; charset=utf-8");
}

void KanbanHandler::handleWithContext(
    const char*, const char*, const char*, void* response, const char*)
{
    auto* res = static_cast<httplib::Response*>(response);
    res->set_content(loadKanbanList(), "application/json; charset=utf-8");
}
