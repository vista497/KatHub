#include "ChatHandler.h"
#include "HermesApiClient.h"
#include "PluginRegistry.h"

#include "httplib.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <memory>
#include <chrono>

ChatHandler::ChatHandler() = default;

const char* ChatHandler::route() { return "/api/chat"; }
IHttpHandler::HttpMethod ChatHandler::method() { return HttpMethod::POST; }

void ChatHandler::setApiClient(std::shared_ptr<HermesApiClient> client) { api_ = std::move(client); }

static QString extractReply(const std::string& rawJson)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(rawJson), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return QString::fromStdString(rawJson);
    QJsonObject root = doc.object();
    if (root.contains("error")) return root["error"].toString();
    if (root.contains("message")) {
        QJsonObject msg = root["message"].toObject();
        QString content = msg.value("content").toString();
        if (!content.isEmpty()) return content;
    }
    if (root.contains("reply")) return root["reply"].toString();
    return QString::fromStdString(rawJson);
}

void ChatHandler::handle(const char* request, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);

    if (!api_) {
        res->status = 500;
        res->set_content(R"({"error":"API client not configured"})", "application/json");
        return;
    }

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
    if (sessionId.empty()) {
        // Create new session if none provided
        std::string createResp = api_->createSession();
        QJsonDocument d = QJsonDocument::fromJson(QByteArray::fromStdString(createResp));
        if (d.isObject()) {
            QJsonObject o = d.object();
            sessionId = o.value("session_id").toString().toStdString();
            if (sessionId.empty() && o.contains("session"))
                sessionId = o["session"].toObject().value("id").toString().toStdString();
            if (sessionId.empty())
                sessionId = o.value("id").toString().toStdString();
        }
    }

    // Send to Hermes via API — blocks until full response
    std::string chatResp = api_->chat(sessionId, message.toStdString());

    QJsonObject reply;
    reply["sessionId"] = QString::fromStdString(sessionId);
    reply["reply"] = extractReply(chatResp);
    res->status = 200;

    QByteArray respJson = QJsonDocument(reply).toJson(QJsonDocument::Compact);
    res->set_content(respJson.toStdString(), "application/json; charset=utf-8");
}

REGISTER_HANDLER(ChatHandler)
