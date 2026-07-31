#include "HermesApiClient.h"

#include "httplib.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QString>

#include <algorithm>

HermesApiClient::HermesApiClient(const std::string& baseUrl, const std::string& apiKey)
    : baseUrl_(baseUrl), apiKey_(apiKey) {}

std::string HermesApiClient::request(const std::string& method,
                                      const std::string& path,
                                      const std::string& body)
{
    httplib::Client cli(baseUrl_);
    cli.set_connection_timeout(10);
    cli.set_read_timeout(300);  // 5 min — Hermes with tools takes time

    httplib::Headers headers = {
        {"Authorization", "Bearer " + apiKey_}
    };

    httplib::Result res;
    if (method == "GET")
        res = cli.Get(path, headers);
    else if (method == "POST")
        res = cli.Post(path, headers, body, "application/json");
    else if (method == "PUT")
        res = cli.Put(path, headers, body, "application/json");
    else if (method == "PATCH")
        res = cli.Patch(path, headers, body, "application/json");
    else if (method == "DELETE")
        res = cli.Delete(path, headers);
    else
        return R"({"error":"Unsupported method"})";

    if (!res) {
        QJsonObject err;
        err["error"] = QStringLiteral("Hermes API unreachable: %1")
            .arg(httplib::to_string(res.error()).c_str());
        return QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString();
    }

    if (res->status >= 400) {
        QJsonObject err;
        err["error"] = QStringLiteral("Hermes API error %1: %2")
            .arg(res->status)
            .arg(QString::fromStdString(res->body).left(200));
        return QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString();
    }

    return res->body;
}

// ── Health ──────────────────────────────────────────────────────────────

bool HermesApiClient::isAlive()
{
    httplib::Client cli(baseUrl_);
    cli.set_connection_timeout(3);
    auto res = cli.Get("/health");
    return res && res->status == 200;
}

// ── Sessions ────────────────────────────────────────────────────────────

std::string HermesApiClient::listSessions()
{
    return request("GET", "/api/sessions");
}

std::string HermesApiClient::getMessages(const std::string& sessionId)
{
    return request("GET", "/api/sessions/" + sessionId + "/messages");
}

std::string HermesApiClient::createSession()
{
    return request("POST", "/api/sessions", "{}");
}

std::string HermesApiClient::chat(const std::string& sessionId, const std::string& message)
{
    QJsonObject body;
    body["message"] = QString::fromStdString(message);
    QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return request("POST", "/api/sessions/" + sessionId + "/chat", json.toStdString());
}

std::string HermesApiClient::deleteSession(const std::string& sessionId)
{
    return request("DELETE", "/api/sessions/" + sessionId);
}

// ── Runs (SSE streaming) ───────────────────────────────────────────────

static std::string jsonString(const QJsonObject& obj, const char* key,
                              const std::string& fallback = "")
{
    const QJsonValue v = obj.value(QString::fromLatin1(key));
    return v.isString() ? v.toString().toStdString() : fallback;
}

std::string HermesApiClient::startRun(const std::string& sessionId,
                                      const std::string& message)
{
    QJsonObject body;
    body["input"] = QString::fromStdString(message);
    if (!sessionId.empty())
        body["session_id"] = QString::fromStdString(sessionId);

    QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    std::string resp = request("POST", "/v1/runs", json.toStdString());

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromStdString(resp), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        std::string runId = jsonString(doc.object(), "run_id");
        if (!runId.empty())
            return runId;
    }
    // Fallback: return the raw body; caller reports it as an error string.
    return resp;
}

std::string HermesApiClient::streamRunEvents(const std::string& runId,
                                             const RunEventCallback& onEvent)
{
    httplib::Client cli(baseUrl_);
    cli.set_connection_timeout(10);
    cli.set_read_timeout(60);  // per-chunk; SSE keepalives arrive every ~30s

    httplib::Headers headers = {
        {"Authorization", "Bearer " + apiKey_},
        {"Accept", "text/event-stream"},
    };

    std::string buffer;       // partial SSE frame
    std::string finalOutput;  // run.completed output / run.failed error
    bool terminalSeen = false;

    const std::string path = "/v1/runs/" + runId + "/events";
    httplib::Result res = cli.Get(path, headers,
        [&](const char* data, size_t len) -> bool {
            buffer.append(data, len);
            // Split on SSE frame boundary ("\n\n" or "\r\n\r\n").
            size_t pos;
            while ((pos = buffer.find("\n\n")) != std::string::npos) {
                std::string frame = buffer.substr(0, pos);
                buffer.erase(0, pos + 2);

                // Extract the "data: " payload (may span multiple data: lines).
                std::string payload;
                size_t dpos = 0;
                while ((dpos = frame.find("data:")) != std::string::npos) {
                    size_t lineEnd = frame.find('\n', dpos);
                    std::string line = frame.substr(
                        dpos + 5, lineEnd == std::string::npos
                            ? std::string::npos : lineEnd - dpos - 5);
                    // Strip a trailing '\r' left by CRLF frames.
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();
                    payload += line;
                    if (lineEnd == std::string::npos)
                        break;
                    frame.erase(0, lineEnd + 1);
                }
                if (payload.empty() || payload[0] != '{')
                    continue;  // keepalive comment or non-JSON

                QJsonParseError err;
                QJsonDocument doc = QJsonDocument::fromJson(
                    QByteArray::fromStdString(payload), &err);
                if (err.error != QJsonParseError::NoError || !doc.isObject())
                    continue;

                QJsonObject event = doc.object();
                std::string etype = jsonString(event, "event");

                if (etype == "run.completed") {
                    finalOutput = jsonString(event, "output");
                    terminalSeen = true;
                } else if (etype == "run.failed") {
                    finalOutput = jsonString(event, "error", "Agent run failed");
                    terminalSeen = true;
                } else if (etype == "run.cancelled") {
                    finalOutput = "Run cancelled";
                    terminalSeen = true;
                }

                if (onEvent)
                    onEvent(event);

                if (terminalSeen)
                    return false;  // stop reading the stream
            }
            return true;
        });

    if (!res) {
        QJsonObject err;
        err["error"] = QStringLiteral("SSE stream failed: %1")
            .arg(httplib::to_string(res.error()).c_str());
        return QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString();
    }
    if (res->status >= 400) {
        QJsonObject err;
        err["error"] = QStringLiteral("SSE stream HTTP %1: %2")
            .arg(res->status)
            .arg(QString::fromStdString(res->body).left(200));
        return QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString();
    }
    return finalOutput;
}

std::string HermesApiClient::resolveRunApproval(const std::string& runId,
                                                const std::string& choice,
                                                bool all)
{
    QJsonObject body;
    body["choice"] = QString::fromStdString(choice);
    if (all)
        body["all"] = true;
    QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return request("POST", "/v1/runs/" + runId + "/approval", json.toStdString());
}

// ── Cron ────────────────────────────────────────────────────────────────

std::string HermesApiClient::listCron()
{
    return request("GET", "/api/cron");
}

std::string HermesApiClient::getCron(const std::string& name)
{
    return request("GET", "/api/cron/" + name);
}

std::string HermesApiClient::createCron(const std::string& config)
{
    return request("POST", "/api/cron", config);
}

std::string HermesApiClient::deleteCron(const std::string& name)
{
    return request("DELETE", "/api/cron/" + name);
}

std::string HermesApiClient::toggleCron(const std::string& name)
{
    return request("PATCH", "/api/cron/" + name + "/toggle");
}

std::string HermesApiClient::runCron(const std::string& name)
{
    return request("POST", "/api/cron/" + name + "/run");
}

// ── Skills ──────────────────────────────────────────────────────────────

std::string HermesApiClient::listSkills()
{
    return request("GET", "/api/skills");
}

std::string HermesApiClient::getSkill(const std::string& name)
{
    return request("GET", "/api/skills/" + name);
}

std::string HermesApiClient::createSkill(const std::string& config)
{
    return request("POST", "/api/skills", config);
}

std::string HermesApiClient::updateSkill(const std::string& name, const std::string& config)
{
    return request("PUT", "/api/skills/" + name, config);
}

std::string HermesApiClient::deleteSkill(const std::string& name)
{
    return request("DELETE", "/api/skills/" + name);
}

// ── Models ──────────────────────────────────────────────────────────────

std::string HermesApiClient::listModels()
{
    return request("GET", "/api/models");
}

std::string HermesApiClient::getModel(const std::string& name)
{
    return request("GET", "/api/models/" + name);
}

std::string HermesApiClient::switchModel(const std::string& modelName)
{
    QJsonObject body;
    body["model"] = QString::fromStdString(modelName);
    QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return request("POST", "/api/models/switch", json.toStdString());
}

// ── System ──────────────────────────────────────────────────────────────

std::string HermesApiClient::getSystemStatus()
{
    return request("GET", "/api/system/status");
}

// ── Profiles ────────────────────────────────────────────────────────────

std::string HermesApiClient::listProfiles()
{
    return request("GET", "/api/profiles");
}

std::string HermesApiClient::getProfileStatus(const std::string& profileName)
{
    return request("GET", "/api/profiles/" + profileName + "/status");
}
