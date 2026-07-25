#include "ChatHandler.h"
#include "HermesApiClient.h"
#include "PluginRegistry.h"

#include "httplib.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QDir>

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

    // Use hermes send to forward the message to Telegram.
    // The gateway will pick it up and process it as a normal incoming message.
    // Target: telegram:329649100 (Мишка Успенский's DM)

    QString hermesPath = QDir::toNativeSeparators(
        QDir::homePath() + "/AppData/Local/hermes/hermes-agent/venv/Scripts/python.exe");
    QStringList args;
    args << "-m" << "hermes_cli.main" << "send"
         << "--to" << "telegram:329649100"
         << message;

    QProcess proc;
    proc.start(hermesPath, args);
    bool finished = proc.waitForFinished(15000); // 15s timeout

    QJsonObject reply;
    if (finished && proc.exitCode() == 0) {
        reply["status"] = QStringLiteral("sent");
        reply["message"] = QStringLiteral("Message forwarded to Telegram. Response will appear shortly.");
        res->status = 200;
    } else {
        QString err = proc.readAllStandardError();
        reply["status"] = QStringLiteral("error");
        reply["error"] = QStringLiteral("Failed to send: %1").arg(err.left(200));
        res->status = 500;
    }

    // Include session ID so frontend can track
    QString sessionId = reqObj.value("sessionId").toString();
    if (!sessionId.isEmpty()) {
        reply["sessionId"] = sessionId;
    }

    QByteArray respJson = QJsonDocument(reply).toJson(QJsonDocument::Compact);
    res->set_content(respJson.toStdString(), "application/json; charset=utf-8");
}

REGISTER_HANDLER(ChatHandler)
