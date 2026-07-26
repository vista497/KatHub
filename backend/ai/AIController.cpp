#include "AIController.h"
#include "Conversation.h"
#include "ToolDispatcher.h"
#include "core/AIService.h"
#include "core/SignalHub.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>

#include <thread>
#include <string>

namespace KatHub {

// ============================================================================
// Construction
// ============================================================================

AIController::AIController(QObject *parent)
    : QObject(parent)
    , m_conversation(new Conversation(this))
    , m_toolDispatcher(new ToolDispatcher(this))
{
    // Forward tool dispatcher signals
    connect(m_toolDispatcher, &ToolDispatcher::toolCalled,
            this, &AIController::toolCallDetected);
    connect(m_toolDispatcher, &ToolDispatcher::toolResult,
            this, &AIController::toolCallCompleted);
}

AIController::~AIController()
{
    cancelRequest();
}

// ============================================================================
// Dependency injection
// ============================================================================

void AIController::setAIService(AIService *service)
{
    m_aiService = service;
}

void AIController::setSignalHub(SignalHub *hub)
{
    m_signalHub = hub;
    m_toolDispatcher->setSignalHub(hub);
}

// ============================================================================
// sendMessage
// ============================================================================

void AIController::sendMessage(const QString &text)
{
    if (!m_aiService) {
        emit error(QStringLiteral("AIService not set — call setAIService() first"));
        return;
    }

    if (text.trimmed().isEmpty())
        return;

    // Cancel any in-flight request
    cancelRequest();

    m_busy = true;
    int reqId = ++m_requestId;
    m_activeRequestId.store(reqId);

    emit requestStarted();

    // Add user message to conversation
    m_conversation->addMessage(Conversation::Role::User, text);

    // Build full context from conversation history
    QJsonArray context = m_conversation->getContext();
    QJsonDocument contextDoc(context);
    QString prompt = QString::fromUtf8(
        contextDoc.toJson(QJsonDocument::Compact));

    qDebug() << "[AIController] sendMessage — context messages:"
             << m_conversation->messageCount()
             << "prompt size:" << prompt.size() << "bytes";

    // Publish event via SignalHub
    if (m_signalHub) {
        QJsonObject eventData;
        eventData[QStringLiteral("action")]   = QStringLiteral("send");
        eventData[QStringLiteral("message")]  = text;
        eventData[QStringLiteral("msgCount")] = m_conversation->messageCount();
        m_signalHub->publish(QStringLiteral("ai:request:started"), eventData);
    }

    // Run the synchronous chat() call on a background thread.
    // Results are delivered back to the main thread via QMetaObject::invokeMethod.
    AIService *service     = m_aiService;
    std::string promptStr  = prompt.toStdString();

    std::thread([this, service, promptStr, reqId]() {
        std::string result = service->chat(promptStr);

        // Deliver result back to main thread
        QString qResult = QString::fromStdString(result);
        QMetaObject::invokeMethod(this, [this, qResult, reqId]() {
            onChatResult(qResult);
        }, Qt::QueuedConnection);
    }).detach();
}

// ============================================================================
// cancelRequest
// ============================================================================

void AIController::cancelRequest()
{
    // Increment request ID to invalidate any in-flight results
    m_activeRequestId.store(0);

    if (m_busy) {
        m_busy = false;
        emit requestFinished();
    }
}

// ============================================================================
// onChatResult — called on main thread when async chat() completes
// ============================================================================

void AIController::onChatResult(const QString &response)
{
    // Discard stale results from cancelled requests
    if (m_activeRequestId.load() == 0) {
        qDebug() << "[AIController] Discarding stale result (request was cancelled)";
        return;
    }

    qDebug() << "[AIController] Response received, length:" << response.size();

    processResponse(response);

    m_busy = false;
    emit requestFinished();
}

// ============================================================================
// processResponse
// ============================================================================

void AIController::processResponse(const QString &response)
{
    // Check for error prefix
    if (response.startsWith(QStringLiteral("ERROR:"))) {
        QString errMsg = response.mid(6).trimmed();
        qWarning() << "[AIController] Backend error:" << errMsg;

        emit error(errMsg);

        if (m_signalHub) {
            QJsonObject eventData;
            eventData[QStringLiteral("error")] = errMsg;
            m_signalHub->publish(QStringLiteral("ai:request:error"), eventData);
        }
        return;
    }

    // Emit streaming (full text as one chunk for synchronous providers)
    emit streaming(response);

    // Add assistant response to conversation
    m_conversation->addMessage(Conversation::Role::Assistant, response);

    // Parse and dispatch any tool calls embedded in the response
    QJsonArray toolResults = m_toolDispatcher->parseAndDispatch(response);

    // If tools were executed, add their results to conversation
    for (const auto &val : toolResults) {
        QJsonObject result = val.toObject();
        QString toolName = result[QStringLiteral("_tool_name")].toString();
        if (toolName.isEmpty())
            continue;

        // Add tool result as a tool message
        QString resultStr = QString::fromUtf8(
            QJsonDocument(result).toJson(QJsonDocument::Compact));
        m_conversation->addMessage(
            Conversation::Role::Tool, resultStr, toolName, toolName);
    }

    // Emit text ready
    emit textReady(response);

    // Publish event via SignalHub
    if (m_signalHub) {
        QJsonObject eventData;
        eventData[QStringLiteral("action")]    = QStringLiteral("response");
        eventData[QStringLiteral("response")]  = response;
        eventData[QStringLiteral("length")]    = response.size();
        eventData[QStringLiteral("toolCalls")] = toolResults.size();
        m_signalHub->publish(QStringLiteral("ai:response:ready"), eventData);
    }
}

// ============================================================================
// Convenience
// ============================================================================

void AIController::setSystemPrompt(const QString &prompt)
{
    m_conversation->setSystemPrompt(prompt);
}

void AIController::clearConversation()
{
    m_conversation->clear();
}

} // namespace KatHub
