#include "ChatHandler.h"
#include "HermesApiClient.h"
#include "PluginRegistry.h"

#include "httplib.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <memory>
#include <chrono>
#include <iostream>

ChatHandler::ChatHandler() = default;

const char* ChatHandler::route() { return "/api/chat"; }

IHttpHandler::HttpMethod ChatHandler::method() { return HttpMethod::POST; }

void ChatHandler::setApiClient(std::shared_ptr<HermesApiClient> client)
{
    api_ = std::move(client);
}

void ChatHandler::handle(const char* request, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);

    if (!api_) {
        res->set_content(R"({"error":"Hermes API client not configured"})", "application/json");
        res->status = 500;
        return;
    }

    // Parse incoming JSON: {"message":"...", "sessionId":"..."}
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
        res->set_content(R"({"error":"Missing 'message' field"})", "application/json");
        return;
    }

    QString sessionId = reqObj.value("sessionId").toString();
    std::string sid = sessionId.toStdString();

    // If no session, create one via Hermes API
    if (sid.empty()) {
        std::string createResp = api_->createSession();
        QJsonDocument cd = QJsonDocument::fromJson(QByteArray::fromStdString(createResp));
        if (cd.isObject()) {
            QJsonObject obj = cd.object();
            // Hermes API returns {"session": {"id": "api_..."}}
            if (obj.contains("session") && obj["session"].isObject()) {
                sid = obj["session"].toObject()["id"].toString().toStdString();
            } else if (obj.contains("session_id")) {
                sid = obj["session_id"].toString().toStdString();
            }
        }
        if (sid.empty()) {
            sid = "kat-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count());
        }
    }

    // Forward to Hermes API
    std::string replyJson = api_->chat(sid, message.toStdString());

    // Inject sessionId into response so frontend can track it
    QJsonDocument replyDoc = QJsonDocument::fromJson(QByteArray::fromStdString(replyJson));
    QJsonObject replyObj;
    if (replyDoc.isObject()) {
        replyObj = replyDoc.object();
    }
    replyObj["sessionId"] = QString::fromStdString(sid);

    QByteArray respJson = QJsonDocument(replyObj).toJson(QJsonDocument::Compact);
    res->set_content(respJson.toStdString(), "application/json; charset=utf-8");
}

REGISTER_HANDLER(ChatHandler)
