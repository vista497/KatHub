#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace KatHub {

/// Manages conversation history for an AI chat session.
///
/// Stores messages as OpenAI-compatible JSON objects (role + content).
/// Provides context trimming to fit within token limits.
class Conversation : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Conversation)

public:
    /// Message role enum matching OpenAI chat format.
    enum class Role {
        System,
        User,
        Assistant,
        Tool
    };
    Q_ENUM(Role)

    explicit Conversation(QObject *parent = nullptr);

    /// Add a message to the conversation history.
    void addMessage(Role role, const QString &content,
                    const QString &toolCallId = {},
                    const QString &toolName = {});

    /// Get the full conversation context as a JSON array of messages.
    QJsonArray getContext() const;

    /// Trim the conversation to fit within maxTokens, reserving space
    /// for the expected response. Removes oldest non-system messages first.
    void trimToFit(int maxTokens, int reserveForResponse = 1024);

    /// Clear all messages (system prompt is preserved).
    void clear();

    /// Number of messages in the conversation.
    int messageCount() const;

    /// Set the system prompt. Replaces any existing system message.
    void setSystemPrompt(const QString &prompt);

    /// Get the current system prompt.
    QString systemPrompt() const;

    /// Get message at the given index.
    QJsonObject messageAt(int index) const;

signals:
    void messageAdded(const QJsonObject &msg);
    void cleared();

private:
    static QString roleToString(Role role);
    static Role stringToRole(const QString &str);
    int estimateTokens(const QString &text) const;

    QJsonArray m_messages;
    QString    m_systemPrompt;
    int        m_systemMsgIndex = -1; // index of system message, -1 if not present
};

} // namespace KatHub
