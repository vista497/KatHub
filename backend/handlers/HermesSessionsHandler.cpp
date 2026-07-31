#include "HermesSessionsHandler.h"
#include "HermesApiClient.h"
#include "HermesCliHelper.h"
#include "httplib.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>
#include <QStringList>

#include <cstring>
#include <sstream>
#include <vector>

HermesSessionsHandler::HermesSessionsHandler() = default;

const char* HermesSessionsHandler::route() { return "/api/hermes/sessions"; }
IHttpHandler::HttpMethod HermesSessionsHandler::method() { return HttpMethod::GET; }

// Parse the human-readable table printed by `hermes sessions list`.
// The table is SPACE-ALIGNED (no '|' separators):
//   Preview   Workspace   Last Active   Src   ID
// Columns are located from the header row's token positions, so both
// 4-column (no Src) and 5-column layouts parse correctly.
// Returns a JSON array of {id, title, workspace, last_active, source}.
static std::string parseSessionsTable(const std::string& table)
{
    QJsonArray out;
    std::istringstream ss(table);
    std::string line;
    bool headerSeen = false;
    QVector<int> colStarts;
    QStringList header;

    auto allDashes = [](const std::string& l) {
        for (char c : l) {
            if (c != '-' && c != ' ' && c != '\t' && c != '\n') return false;
        }
        return true;
    };

    while (std::getline(ss, line)) {
        if (line.empty() || allDashes(line)) continue;
        if (line.find_first_not_of(" \t\n") == std::string::npos) continue;  // blank

        if (!headerSeen) {
            headerSeen = true;
            colStarts.clear();
            header.clear();
            size_t i = 0;
            while (i < line.size()) {
                while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
                if (i >= line.size()) break;
                colStarts.append(static_cast<int>(i));
                size_t j = i;
                while (j < line.size() && line[j] != ' ' && line[j] != '\t') ++j;
                header << QString::fromStdString(line.substr(i, j - i));
                i = j;
            }
            continue;
        }

        // Map column indices from the header tokens.
        int idxId = -1, idxLast = -1, idxWs = -1, idxSrc = -1;
        for (int h = 0; h < header.size(); ++h) {
            QString hh = header[h].toLower();
            if (hh == "id") idxId = h;
            else if (hh.contains("last")) idxLast = h;
            else if (hh.contains("workspace")) idxWs = h;
            else if (hh == "src" || hh.contains("source")) idxSrc = h;
        }
        if (idxId < 0) idxId = header.size() - 1;
        if (idxId < 0 || idxId >= colStarts.size()) continue;

        int n = colStarts.size();
        auto slice = [&](int col) -> QString {
            if (col < 0 || col >= n) return QString();
            int start = colStarts[col];
            int end = (col + 1 < n) ? colStarts[col + 1] : static_cast<int>(line.size());
            if (start >= static_cast<int>(line.size())) return QString();
            end = qMin(end, static_cast<int>(line.size()));
            return QString::fromStdString(line.substr(static_cast<size_t>(start),
                                                     static_cast<size_t>(end - start))).trimmed();
        };

        QString id = slice(idxId);
        if (id.isEmpty()) continue;
        QString workspace = slice(idxWs);
        QString lastActive = slice(idxLast);
        QString src = slice(idxSrc);

        // Title = everything before the Workspace column (fallback: before ID).
        QString title;
        int titleEnd = (idxWs > 0) ? colStarts[idxWs] : colStarts[idxId];
        if (titleEnd > 0 && titleEnd <= static_cast<int>(line.size()))
            title = QString::fromStdString(line.substr(0, static_cast<size_t>(titleEnd))).trimmed();
        if (title.isEmpty() || title == "—") title = id;

        QJsonObject s;
        s["id"] = id;
        s["session_id"] = id;
        s["title"] = title;
        s["workspace"] = workspace;
        s["last_active"] = lastActive;
        s["updated_at"] = lastActive;   // display string ("3m ago") — fine for UI
        s["source"] = src;
        out.append(s);
    }
    return QJsonDocument(out).toJson(QJsonDocument::Compact).toStdString();
}

void HermesSessionsHandler::handle(const char* request, void* response)
{
    handleWithContext(request, "/api/hermes/sessions", nullptr, response, "GET");
}

void HermesSessionsHandler::handleWithContext(const char*, const char* path, const char*, void* response, const char*)
{
    auto* res = static_cast<httplib::Response*>(response);

    std::string pathStr(path ? path : "/api/hermes/sessions");
    std::string prefix = "/api/hermes/sessions";
    std::string rest;
    if (pathStr.size() > prefix.size() && pathStr.rfind(prefix, 0) == 0) {
        rest = pathStr.substr(prefix.size());
        if (!rest.empty() && rest[0] == '/') rest = rest.substr(1);
    }

    if (rest.empty()) {
        // ── GET /api/hermes/sessions — list ──
        // Prefer the Hermes API server (structured JSON, correct ids).
        // Fall back to parsing `hermes sessions list` when api_ is missing
        // or the API errored (the table parser is fragile: long titles and
        // BOM'd dash separators shift columns and corrupt ids).
        if (api_) {
            QJsonParseError perr;
            QJsonDocument doc = QJsonDocument::fromJson(
                QByteArray::fromStdString(api_->listSessions()), &perr);
            if (perr.error == QJsonParseError::NoError && doc.isObject() &&
                !doc.object().contains("error")) {
                QJsonArray data = doc.object().value("data").toArray();
                QJsonArray out;
                for (const QJsonValue& v : data) {
                    QJsonObject s = v.toObject();
                    QString id = s.value("id").toString();
                    if (id.isEmpty()) continue;
                    QString title = s.value("title").toString();
                    if (title.isEmpty() || title == "null") title = id;
                    QJsonObject o;
                    o["id"] = id;
                    o["session_id"] = id;
                    o["title"] = title;
                    o["workspace"] = QString();
                    o["last_active"] = s.value("updated_at").toString();
                    o["updated_at"] = s.value("updated_at").toString();
                    o["source"] = s.value("source").toString();
                    o["message_count"] = s.value("message_count").toInt(0);
                    out.append(o);
                }
                res->status = 200;
                res->set_content(
                    QJsonDocument(out).toJson(QJsonDocument::Compact).toStdString(),
                    "application/json; charset=utf-8");
                return;
            }
        }

        auto r = HermesCliHelper::runArgv({"sessions", "list"});
        if (r.exitCode != 0) {
            res->status = 502;
            QJsonObject err;
            err["error"] = QString::fromStdString(r.stderrText.empty()
                ? "hermes sessions list failed" : r.stderrText);
            res->set_content(QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString(),
                             "application/json; charset=utf-8");
            return;
        }
        res->status = 200;
        res->set_content(parseSessionsTable(r.stdoutText), "application/json; charset=utf-8");
        return;
    }

    // ── GET /api/hermes/sessions/<id> — messages ──
    // Same preference: Hermes API first, CLI export as fallback.
    if (api_) {
        QJsonParseError perr;
        QJsonDocument doc = QJsonDocument::fromJson(
            QByteArray::fromStdString(api_->getMessages(rest)), &perr);
        if (perr.error == QJsonParseError::NoError && doc.isObject() &&
            !doc.object().contains("error")) {
            QJsonArray msgs = doc.object().value("data").toArray();
            res->status = 200;
            res->set_content(QJsonDocument(msgs).toJson(QJsonDocument::Compact).toStdString(),
                             "application/json; charset=utf-8");
            return;
        }
    }

    auto r = HermesCliHelper::runArgv({"sessions", "export",
                                       "--session-id", rest,
                                       "--format", "jsonl", "-"});
    if (r.exitCode != 0) {
        res->status = 502;
        QJsonObject err;
        err["error"] = QString::fromStdString(r.stderrText.empty()
            ? "hermes sessions export failed" : r.stderrText);
        res->set_content(QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString(),
                         "application/json; charset=utf-8");
        return;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromStdString(r.stdoutText), &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        res->status = 502;
        QJsonObject err;
        err["error"] = "Failed to parse hermes session export";
        res->set_content(QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString(),
                         "application/json; charset=utf-8");
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray msgs = root.value("messages").toArray();
    res->status = 200;
    res->set_content(QJsonDocument(msgs).toJson(QJsonDocument::Compact).toStdString(),
                     "application/json; charset=utf-8");
}

void HermesSessionsHandler::setApiClient(std::shared_ptr<HermesApiClient> client)
{
    api_ = std::move(client);
}
