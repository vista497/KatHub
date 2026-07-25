#include "ChatHandler.h"
#include "PluginRegistry.h"
#include "AIController.h"
#include "AgentProfile.h"
#include "PromptManager.h"

#include "httplib.h"

// windows.h (pulled in by httplib.h) defines DELETE as a macro.
#ifdef DELETE
#undef DELETE
#endif

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QEventLoop>
#include <QTimer>

#include <iostream>

// ---------------------------------------------------------------------------
// Auto-registration via REGISTER_HANDLER macro (runs before main)
// ---------------------------------------------------------------------------
REGISTER_HANDLER(ChatHandler)

// ---------------------------------------------------------------------------
ChatHandler::ChatHandler() = default;

const char *ChatHandler::route()
{
    return "/api/chat";
}

IHttpHandler::HttpMethod ChatHandler::method()
{
    return IHttpHandler::HttpMethod::POST;
}

void ChatHandler::setAIController(KatHub::AIController *ctrl)
{
    m_aiCtrl = ctrl;
}

void ChatHandler::setPromptManager(KatHub::PromptManager *pm)
{
    m_promptMgr = pm;
}

void ChatHandler::handle(const char *request, void *response)
{
    auto *res = static_cast<httplib::Response *>(response);

    // --- Validate dependencies ---
    if (!m_aiCtrl) {
        // Fallback: echo mode when AI Controller is not configured.
        // Simply echo the raw request body back.
        QJsonObject echoObj;
        echoObj[QStringLiteral("reply")] =
            QStringLiteral("[Echo mode] AI engine offline. Request received: %1 bytes")
                .arg(static_cast<int>(strlen(request)));
        QByteArray echoJson = QJsonDocument(echoObj).toJson(QJsonDocument::Compact);
        res->set_content(echoJson.toStdString(), "application/json");
        return;
    }

    // --- Parse request JSON ---
    QJsonParseError parseErr;
    QJsonDocument reqDoc = QJsonDocument::fromJson(
        QByteArray::fromRawData(request, static_cast<int>(strlen(request))),
        &parseErr);

    if (parseErr.error != QJsonParseError::NoError || !reqDoc.isObject()) {
        res->status = 400;
        res->set_content(R"({"error":"Invalid JSON"})", "application/json");
        return;
    }

    QJsonObject reqObj = reqDoc.object();
    QString message = reqObj.value(QStringLiteral("message")).toString().trimmed();
    if (message.isEmpty()) {
        res->status = 400;
        res->set_content(R"({"error":"Missing 'message' field"})", "application/json");
        return;
    }

    QString agentName = reqObj.value(QStringLiteral("agent")).toString(QStringLiteral("default"));

    // --- Resolve agent profile and system prompt ---
    // Load profiles (use defaults if no file specified; ChatHandler uses
    // PromptManager's base directory for template resolution).
    QString systemPrompt;
    QList<KatHub::AgentProfile> profiles;
    if (m_promptMgr) {
        // Try loading profiles from the prompt manager's base directory
        QString profilesPath = m_promptMgr->baseDir() + QStringLiteral("/profiles.json");
        profiles = KatHub::AgentProfile::loadFromFile(profilesPath);
    }
    if (profiles.isEmpty()) {
        profiles = KatHub::AgentProfile::defaults();
    }

    KatHub::AgentProfile profile = KatHub::AgentProfile::find(profiles, agentName);
    if (profile.name.isEmpty()) {
        res->status = 400;
        QJsonObject err;
        err[QStringLiteral("error")] =
            QStringLiteral("Unknown agent: %1").arg(agentName);
        QByteArray errJson = QJsonDocument(err).toJson(QJsonDocument::Compact);
        res->set_content(errJson.toStdString(), "application/json");
        return;
    }

    // Resolve system prompt: if it's a file path, load via PromptManager
    if (profile.isSystemPromptPath() && m_promptMgr) {
        // Extract template name from path (e.g. "templates/default-system")
        QString tmplName = profile.systemPrompt;
        if (tmplName.endsWith(QStringLiteral(".md")) ||
            tmplName.endsWith(QStringLiteral(".txt"))) {
            // Remove extension for PromptManager::process
            tmplName.chop(tmplName.contains(QStringLiteral(".md")) ? 3 : 4);
        }
        systemPrompt = m_promptMgr->process(tmplName);
    } else {
        systemPrompt = profile.systemPrompt;
    }

    // If system prompt is still empty, use a fallback
    if (systemPrompt.isEmpty()) {
        systemPrompt = QStringLiteral("You are a helpful assistant.");
    }

    // --- Set system prompt on conversation ---
    m_aiCtrl->setSystemPrompt(systemPrompt);

    // --- Send message and wait for response ---
    // AIController::sendMessage is asynchronous; use QEventLoop to block
    // the handler thread until textReady or a timeout.
    QString reply;
    bool gotReply = false;
    bool gotError = false;
    QString errorMsg;

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    // Capture the signals before sending
    QMetaObject::Connection connText =
        QObject::connect(m_aiCtrl, &KatHub::AIController::textReady,
            [&](const QString &text) {
                reply = text;
                gotReply = true;
                loop.quit();
            });

    QMetaObject::Connection connErr =
        QObject::connect(m_aiCtrl, &KatHub::AIController::error,
            [&](const QString &msg) {
                errorMsg = msg;
                gotError = true;
                loop.quit();
            });

    QMetaObject::Connection connTimeout =
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    // 30-second timeout for AI response
    timeout.start(30000);

    // Fire the request
    m_aiCtrl->sendMessage(message);

    // Block until response, error, or timeout
    loop.exec();

    // Clean up connections
    QObject::disconnect(connText);
    QObject::disconnect(connErr);
    QObject::disconnect(connTimeout);

    // --- Build response ---
    QJsonObject respObj;
    respObj[QStringLiteral("agent")] = agentName;

    if (gotError) {
        respObj[QStringLiteral("reply")] =
            QStringLiteral("Error: %1").arg(errorMsg);
        res->status = 500;
    } else if (!gotReply) {
        respObj[QStringLiteral("reply")] =
            QStringLiteral("Request timed out after 30 seconds.");
        res->status = 504;
    } else {
        respObj[QStringLiteral("reply")] = reply;
        res->status = 200;
    }

    QByteArray respJson = QJsonDocument(respObj).toJson(QJsonDocument::Compact);
    res->set_content(respJson.toStdString(), "application/json");
}
