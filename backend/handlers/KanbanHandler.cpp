#include "KanbanHandler.h"
#include "HermesCliHelper.h"

#include "httplib.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>

#include <cstring>
#include <sstream>
#include <regex>
#include <vector>

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

// Resolve the kanban_move.py script next to the backend binary:
//   installed: <exe_dir>/scripts/kanban_move.py
//   dev:       <exe_dir>/../../../backend/scripts/kanban_move.py
static std::string resolveMoveScript()
{
    QString exeDir = QCoreApplication::applicationDirPath();
    QString installed = QDir(exeDir).filePath("scripts/kanban_move.py");
    if (QFileInfo::exists(installed)) return installed.toStdString();
    QString dev = QDir(exeDir).filePath("../../../backend/scripts/kanban_move.py");
    if (QFileInfo::exists(dev)) return dev.toStdString();
    return "";
}

// POST /api/kanban/<id>/status  body: {"status": "done"}
static std::string moveTask(const std::string& taskId, const std::string& newStatus)
{
    std::string script = resolveMoveScript();
    if (script.empty()) {
        QJsonObject err;
        err["ok"] = false;
        err["error"] = "kanban_move.py not found next to backend";
        return QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString();
    }
    std::string raw = HermesCliHelper::runPython(script, {taskId, newStatus});
    if (raw.empty()) {
        QJsonObject err;
        err["ok"] = false;
        err["error"] = "kanban_move.py failed (exit != 0)";
        return QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString();
    }
    return raw;
}

void KanbanHandler::handle(const char*, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);
    res->set_content(loadKanbanList(), "application/json; charset=utf-8");
}

void KanbanHandler::handleWithContext(
    const char* body, const char* path, const char* /*query*/,
    void* response, const char* method)
{
    auto* res = static_cast<httplib::Response*>(response);
    const std::string pathStr = path ? path : "";

    // Status move: POST/PUT/PATCH /api/kanban/<id>/status
    std::regex moveRe(R"(^/api/kanban/(t_[A-Za-z0-9]+)/status$)");
    std::smatch m;
    bool isWrite = method && (std::strcmp(method, "POST") == 0
                              || std::strcmp(method, "PUT") == 0
                              || std::strcmp(method, "PATCH") == 0);
    if (isWrite && std::regex_match(pathStr, m, moveRe)) {
        std::string taskId = m[1].str();
        std::string newStatus;
        if (body && *body) {
            QJsonParseError parseErr;
            QJsonDocument doc = QJsonDocument::fromJson(body, &parseErr);
            if (parseErr.error == QJsonParseError::NoError && doc.isObject()) {
                newStatus = doc.object().value("status").toString().toStdString();
            }
        }
        if (newStatus.empty()) {
            QJsonObject err;
            err["ok"] = false;
            err["error"] = "missing status in body {\"status\": \"...\"}";
            res->set_content(QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString(),
                             "application/json; charset=utf-8");
            return;
        }
        res->set_content(moveTask(taskId, newStatus), "application/json; charset=utf-8");
        return;
    }

    // Default: return the task list
    res->set_content(loadKanbanList(), "application/json; charset=utf-8");
}
