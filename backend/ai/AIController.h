#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <atomic>

namespace KatHub {

class AIService;
class Conversation;
class ToolDispatcher;
class SignalHub;

/// Main AI controller — bridges user messages, AI backends, conversation
/// history, and tool execution.
///
/// Usage:
/// @code
///   auto *ctrl = new AIController(this);
///   ctrl->setAIService(aiService);
///   ctrl->setSignalHub(signalHub);
///   ctrl->conversation()->setSystemPrompt("You are a helpful assistant.");
///
///   connect(ctrl, &AIController::streaming, ...);
///   connect(ctrl, &AIController::textReady, ...);
///   connect(ctrl, &AIController::error, ...);
///
///   ctrl->sendMessage("Hello!");
/// @endcode
class AIController : public QObject
{
    Q_OBJECT

public:
    explicit AIController(QObject *parent = nullptr);
    ~AIController() override;

    /// Set the AI service (required before sendMessage).
    /// AIController does NOT take ownership.
    void setAIService(AIService *service);

    /// Set the SignalHub for publishing AI lifecycle events.
    void setSignalHub(SignalHub *hub);

    /// Access the conversation history manager.
    Conversation *conversation() const { return m_conversation; }

    /// Access the tool dispatcher.
    ToolDispatcher *toolDispatcher() const { return m_toolDispatcher; }

    /// Send a user message to the AI.
    /// Adds the message to conversation history, builds context,
    /// calls the AI backend asynchronously, parses the response,
    /// dispatches any tool calls, and emits signals.
    void sendMessage(const QString &text);

    /// Cancel the currently in-flight request.
    /// The active chat() call will still complete (no true cancellation
    /// with synchronous providers), but its result will be discarded.
    void cancelRequest();

    /// Set the system prompt (convenience, delegates to Conversation).
    void setSystemPrompt(const QString &prompt);

    /// Clear conversation history.
    void clearConversation();

    /// Returns true if a request is currently in flight.
    bool isBusy() const { return m_busy; }

signals:
    /// Emitted when a text chunk is available (streaming simulation).
    /// With synchronous providers, the full text is emitted as one chunk.
    void streaming(const QString &chunk);

    /// Emitted when the full AI response text is ready.
    void textReady(const QString &fullText);

    /// Emitted when an error occurs.
    void error(const QString &message);

    /// Emitted when a tool call is detected in the AI response.
    void toolCallDetected(const QString &name, const QJsonObject &args);

    /// Emitted when a tool call completes.
    void toolCallCompleted(const QString &name, const QJsonObject &result);

    /// Emitted when the AI request starts processing.
    void requestStarted();

    /// Emitted when the AI request finishes (success or error).
    void requestFinished();

private:
    void processResponse(const QString &response);
    void onChatResult(const QString &response);

    AIService       *m_aiService      = nullptr;
    SignalHub       *m_signalHub      = nullptr;
    Conversation    *m_conversation   = nullptr;
    ToolDispatcher  *m_toolDispatcher = nullptr;

    // Async state — requestId increments on each sendMessage so stale
    // results from cancelled/detached threads are discarded.
    std::atomic<int> m_requestId{0};
    std::atomic<int> m_activeRequestId{0};
    std::atomic<bool> m_busy{false};
};

} // namespace KatHub
