#include "ChatHandler.h"
#include "HermesApiClient.h"
#include "PluginRegistry.h"
#include "Logger.h"

#include "httplib.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>

#include <memory>
#include <mutex>
#include <vector>

ChatHandler::ChatHandler() = default;

const char* ChatHandler::route() { return "/api/chat"; }
IHttpHandler::HttpMethod ChatHandler::method() { return HttpMethod::ALL; }

void ChatHandler::setApiClient(std::shared_ptr<HermesApiClient> client) { api_ = std::move(client); }

void ChatHandler::setProfile(const std::string& profile)
{
    if (!profile.empty()) profile_ = profile;
}

void ChatHandler::handle(const char* request, void* response)
{
    // Backward-compat entry point: treat as POST /api/chat.
    handleWithContext(request, "/api/chat", nullptr, response, "POST");
}

void ChatHandler::handleWithContext(const char* body, const char* path,
                                    const char* query, void* response,
                                    const char* method)
{
    std::string p = path ? path : "/api/chat";
    if (p.rfind("/api/chat/approvals", 0) == 0) {
        handleApprovals(body, query, response, method);
        return;
    }
    if (!method || std::string(method) != "POST") {
        auto* res = static_cast<httplib::Response*>(response);
        res->status = 405;
        res->set_content(R"({"error":"Method not allowed"})", "application/json");
        return;
    }
    handleChat(body, response);
}

// ── POST /api/chat (runs Hermes via /v1/runs SSE so approvals can be
//    surfaced to the UI and resolved in-band) ────────────────────────────

void ChatHandler::handleChat(const char* request, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);

    QJsonParseError parseErr;
    QJsonDocument reqDoc = QJsonDocument::fromJson(
        QByteArray::fromRawData(request, static_cast<int>(strlen(request))), &parseErr);

    if (parseErr.error != QJsonParseError::NoError || !reqDoc.isObject()) {
        res->status = 400;
        res->set_content(R"({"error":"Invalid JSON"})", "application/json");
        return;
    }

    QJsonObject reqObj = reqDoc.object();
    QString message = reqObj.value("message").toString().trimmed();
    if (message.isEmpty()) {
        res->status = 400;
        res->set_content(R"({"error":"Missing message"})", "application/json");
        return;
    }

    std::string sessionId = reqObj.value("sessionId").toString().toStdString();
    bool resume = !sessionId.empty();

    // [ChatSend] ОТЛАДКА ОТПРАВКИ: что реально прислал фронт.
    // Если sessionId пуст — фронт не передал активную сессию, и бэкенд
    // создаст НОВУЮ (это и есть типичная причина «ушло не в ту сессию»).
    {
        QString msgPreview = message.left(80);
        msgPreview.replace('\n', ' ');
        LOG_INFO("[ChatSend] POST /api/chat -> sessionId='" + sessionId
                 + "' resume=" + (resume ? "true" : "false")
                 + " message='" + msgPreview.toStdString() + "'");
    }

    // New session? Create it explicitly via the Hermes API so the id is
    // known up-front (no fragile parsing of `hermes sessions list`).
    if (!resume) {
        if (!api_) {
            res->status = 500;
            res->set_content(R"({"error":"API client not configured"})", "application/json");
            return;
        }
        std::string createResp = api_->createSession();
        QJsonParseError cerr;
        QJsonDocument cdoc = QJsonDocument::fromJson(
            QByteArray::fromStdString(createResp), &cerr);
        if (cerr.error == QJsonParseError::NoError && cdoc.isObject()) {
            QJsonObject o = cdoc.object();
            if (o.contains("session_id"))
                sessionId = o.value("session_id").toString().toStdString();
            else if (o.contains("session"))
                sessionId = o["session"].toObject().value("id").toString().toStdString();
            else if (o.contains("id"))
                sessionId = o.value("id").toString().toStdString();
        }
        if (sessionId.empty()) {
            res->status = 502;
            QJsonObject err;
            err["error"] = QString::fromStdString(
                !createResp.empty() ? createResp : "Failed to create session");
            LOG_ERROR("[ChatSend] createSession FAILED: "
                      + (!createResp.empty() ? createResp : "empty response"));
            res->set_content(QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString(),
                             "application/json; charset=utf-8");
            return;
        }
        LOG_INFO("[ChatSend] создана НОВАЯ сессия (фронт не прислал sessionId): '"
                 + sessionId + "'");
        resume = true;  // continue the freshly created session
    }

    // Send via POST /v1/chat/completions with X-Hermes-Session-Id header —
    // EXACTLY like AI_ControllerApp does. The api_server resumes the session
    // identified by that header (loads its history from state.db), so the
    // reply reliably lands in the session the frontend asked for.
    // (Diagnosed 2026-08-02: /v1/runs ignored the passed session_id and kept
    // using the api_server "current" session; /api/sessions/{id}/chat still
    // showed the wrong session from the user's perspective.)
    QString chatPreview = message.left(80);
    chatPreview.replace('\n', ' ');
    LOG_INFO("[ChatSend] HermesApiClient::chatCompletions session_id='" + sessionId
             + "' -> POST /v1/chat/completions message='"
             + chatPreview.toStdString() + "'");

    std::string output = api_->chatCompletions(sessionId, message.toStdString());

    // chatCompletions() returns the accumulated assistant text, or a
    // JSON {"error": ...} body on transport/HTTP failure. It also appends a
    // "[SESSION_ID=...]" marker line when Hermes rotated the session id.
    QJsonParseError oerr;
    QJsonDocument odoc = QJsonDocument::fromJson(
        QByteArray::fromStdString(output), &oerr);
    QString reply;

    if (oerr.error == QJsonParseError::NoError && odoc.isObject()) {
        QJsonObject obj = odoc.object();
        if (obj.contains("error")) {
            QString errMsg = obj.value("error").toString();
            LOG_ERROR("[ChatSend] chatCompletions FAILED session='" + sessionId
                      + "' -> " + errMsg.toStdString());
            res->status = 502;
            res->set_content(output, "application/json; charset=utf-8");
            return;
        }
    }

    // Strip the optional session-rotation marker before showing the reply.
    reply = QString::fromStdString(output);
    const QString kSessionMarker = "[SESSION_ID=";
    int markerPos = reply.indexOf(kSessionMarker);
    if (markerPos >= 0) {
        QString effSession = reply.mid(markerPos + kSessionMarker.size());
        int end = effSession.indexOf(']');
        if (end >= 0) {
            effSession = effSession.left(end);
            if (!effSession.isEmpty())
                sessionId = effSession.toStdString();
        }
        reply = reply.left(markerPos);
    }

    if (reply.isEmpty() && !output.empty()) {
        // Non-JSON body with no text and no error — transport-level failure.
        LOG_ERROR("[ChatSend] chatCompletions FAILED session='" + sessionId
                  + "' -> non-JSON body: "
                  + QString::fromStdString(output).left(200).toStdString());
        res->status = 502;
        QJsonObject err;
        err["error"] = QString::fromStdString(output);
        res->set_content(QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString(),
                         "application/json; charset=utf-8");
        return;
    }
    if (reply.isEmpty()) {
        LOG_ERROR("[ChatSend] chatCompletions FAILED session='" + sessionId
                  + "' -> пустой ответ от Hermes");
        res->status = 502;
        QJsonObject err;
        err["error"] = "Empty response from Hermes /v1/chat/completions";
        res->set_content(QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString(),
                         "application/json; charset=utf-8");
        return;
    }

    while (!reply.isEmpty() && reply.back().isSpace())
        reply.chop(1);

    {
        QString replyPreview = reply.left(100);
        replyPreview.replace('\n', ' ');
        LOG_INFO("[ChatSend] ОТПРАВЛЕНО в сессию '" + sessionId
                 + "' reply_len=" + std::to_string(reply.size())
                 + " reply_head='" + replyPreview.toStdString() + "'");
    }

    QJsonObject replyObj;
    replyObj["sessionId"] = QString::fromStdString(sessionId);
    replyObj["reply"] = reply;
    res->status = 200;

    QByteArray respJson = QJsonDocument(replyObj).toJson(QJsonDocument::Compact);
    res->set_content(respJson.toStdString(), "application/json; charset=utf-8");
}

// ── GET/POST /api/chat/approvals ────────────────────────────────────────

void ChatHandler::handleApprovals(const char* body, const char* query,
                                  void* response, const char* method)
{
    auto* res = static_cast<httplib::Response*>(response);

    if (!api_) {
        res->status = 500;
        res->set_content(R"({"error":"API client not configured"})", "application/json");
        return;
    }

    std::string m = method ? method : "GET";

    if (m == "GET") {
        // Optional ?sessionId=... — otherwise return the latest pending
        // approval (KatHub has a single chat client, one run at a time).
        std::string wantedSession;
        if (query) {
            std::string q = query;
            size_t pos = q.find("sessionId=");
            if (pos != std::string::npos) {
                wantedSession = q.substr(pos + 10);
                size_t amp = wantedSession.find('&');
                if (amp != std::string::npos)
                    wantedSession.resize(amp);
            }
        }

        std::lock_guard<std::mutex> lock(approvalsMutex_);

        auto fill = [&](const PendingApproval& pa) {
            QJsonObject out;
            out["pending"] = true;
            out["runId"] = QString::fromStdString(pa.runId);
            out["sessionId"] = QString::fromStdString(pa.sessionId);
            out["command"] = QString::fromStdString(pa.command);
            out["description"] = QString::fromStdString(pa.description);
            QJsonArray choices;
            for (const auto& c : pa.choices)
                choices.append(QString::fromStdString(c));
            out["choices"] = choices;
            return out;
        };

        QJsonObject out;
        out["pending"] = false;
        if (!wantedSession.empty()) {
            auto it = pendingBySession_.find(wantedSession);
            if (it != pendingBySession_.end())
                out = fill(it->second);
        } else {
            const PendingApproval* best = nullptr;
            double bestTs = -1.0;
            for (const auto& kv : pendingBySession_) {
                if (kv.second.timestamp > bestTs) {
                    bestTs = kv.second.timestamp;
                    best = &kv.second;
                }
            }
            if (best)
                out = fill(*best);
        }

        res->status = 200;
        res->set_content(QJsonDocument(out).toJson(QJsonDocument::Compact).toStdString(),
                         "application/json; charset=utf-8");
        return;
    }

    if (m == "POST") {
        // Body: {"runId":"...","choice":"once|session|always|deny","all":false}
        QJsonParseError parseErr;
        QJsonDocument reqDoc = QJsonDocument::fromJson(
            QByteArray::fromRawData(body, static_cast<int>(strlen(body))), &parseErr);
        if (parseErr.error != QJsonParseError::NoError || !reqDoc.isObject()) {
            res->status = 400;
            res->set_content(R"({"error":"Invalid JSON"})", "application/json");
            return;
        }
        QJsonObject req = reqDoc.object();
        std::string runId = req.value("runId").toString().toStdString();
        std::string choice = req.value("choice").toString().toStdString();
        if (runId.empty() || choice.empty()) {
            res->status = 400;
            res->set_content(R"({"error":"runId and choice required"})", "application/json");
            return;
        }
        bool all = req.value("all").toBool(false);

        std::string resp = api_->resolveRunApproval(runId, choice, all);

        {
            std::lock_guard<std::mutex> lock(approvalsMutex_);
            auto it = sessionByRun_.find(runId);
            if (it != sessionByRun_.end())
                pendingBySession_.erase(it->second);
            sessionByRun_.erase(runId);
        }

        QJsonObject out;
        QJsonParseError err;
        QJsonDocument d = QJsonDocument::fromJson(QByteArray::fromStdString(resp), &err);
        if (err.error == QJsonParseError::NoError && d.isObject() && d.object().contains("error")) {
            out["error"] = d.object()["error"];
            res->status = 502;
        } else {
            out["resolved"] = true;
            res->status = 200;
        }
        res->set_content(QJsonDocument(out).toJson(QJsonDocument::Compact).toStdString(),
                         "application/json; charset=utf-8");
        return;
    }

    res->status = 405;
    res->set_content(R"({"error":"Method not allowed"})", "application/json");
}

REGISTER_HANDLER(ChatHandler)
