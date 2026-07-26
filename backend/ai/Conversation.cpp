#include "Conversation.h"

#include <QDebug>

namespace KatHub {

// ============================================================================
// Construction
// ============================================================================

Conversation::Conversation(QObject *parent)
    : QObject(parent)
{
}

// ============================================================================
// addMessage
// ============================================================================

void Conversation::addMessage(Role role, const QString &content,
                               const QString &toolCallId,
                               const QString &toolName)
{
    QJsonObject msg;
    msg[QStringLiteral("role")] = roleToString(role);

    if (role == Role::Tool) {
        msg[QStringLiteral("tool_call_id")] = toolCallId;
        msg[QStringLiteral("name")] = toolName;
    }

    msg[QStringLiteral("content")] = content;

    m_messages.append(msg);

    // Update system message index if this is a system message
    if (role == Role::System) {
        m_systemMsgIndex = m_messages.size() - 1;
        m_systemPrompt = content;
    }

    emit messageAdded(msg);
}

// ============================================================================
// getContext
// ============================================================================

QJsonArray Conversation::getContext() const
{
    return m_messages;
}

// ============================================================================
// trimToFit
// ============================================================================

void Conversation::trimToFit(int maxTokens, int reserveForResponse)
{
    int available = maxTokens - reserveForResponse;
    if (available <= 0)
        return;

    // Count total tokens
    int totalTokens = 0;
    for (const auto &val : m_messages) {
        QJsonObject msg = val.toObject();
        totalTokens += estimateTokens(msg[QStringLiteral("content")].toString());
    }

    // Remove oldest non-system messages until we fit
    while (totalTokens > available && m_messages.size() > 0) {
        // Find oldest non-system message
        int removeIdx = -1;
        for (int i = 0; i < m_messages.size(); ++i) {
            QJsonObject msg = m_messages[i].toObject();
            if (msg[QStringLiteral("role")].toString() != QStringLiteral("system")) {
                removeIdx = i;
                break;
            }
        }

        if (removeIdx < 0)
            break; // only system messages left

        QJsonObject removed = m_messages[removeIdx].toObject();
        totalTokens -= estimateTokens(removed[QStringLiteral("content")].toString());

        m_messages.removeAt(removeIdx);

        // Adjust system message index
        if (m_systemMsgIndex > removeIdx)
            --m_systemMsgIndex;

        qDebug() << "[Conversation] Trimmed message at index" << removeIdx
                 << "tokens:" << totalTokens << "/" << available;
    }
}

// ============================================================================
// clear
// ============================================================================

void Conversation::clear()
{
    m_messages = QJsonArray();
    m_systemMsgIndex = -1;

    // Re-add system prompt if set
    if (!m_systemPrompt.isEmpty()) {
        addMessage(Role::System, m_systemPrompt);
    }

    emit cleared();
}

// ============================================================================
// messageCount
// ============================================================================

int Conversation::messageCount() const
{
    return m_messages.size();
}

// ============================================================================
// setSystemPrompt
// ============================================================================

void Conversation::setSystemPrompt(const QString &prompt)
{
    m_systemPrompt = prompt;

    // Remove existing system message if any
    if (m_systemMsgIndex >= 0 && m_systemMsgIndex < m_messages.size()) {
        m_messages.removeAt(m_systemMsgIndex);
        m_systemMsgIndex = -1;
    }

    if (!prompt.isEmpty()) {
        addMessage(Role::System, prompt);
    }
}

// ============================================================================
// systemPrompt
// ============================================================================

QString Conversation::systemPrompt() const
{
    return m_systemPrompt;
}

// ============================================================================
// messageAt
// ============================================================================

QJsonObject Conversation::messageAt(int index) const
{
    if (index >= 0 && index < m_messages.size())
        return m_messages[index].toObject();
    return {};
}

// ============================================================================
// Helpers
// ============================================================================

QString Conversation::roleToString(Role role)
{
    switch (role) {
    case Role::System:    return QStringLiteral("system");
    case Role::User:      return QStringLiteral("user");
    case Role::Assistant: return QStringLiteral("assistant");
    case Role::Tool:      return QStringLiteral("tool");
    }
    return QStringLiteral("user");
}

Conversation::Role Conversation::stringToRole(const QString &str)
{
    if (str == QStringLiteral("system"))    return Role::System;
    if (str == QStringLiteral("user"))      return Role::User;
    if (str == QStringLiteral("assistant")) return Role::Assistant;
    if (str == QStringLiteral("tool"))      return Role::Tool;
    return Role::User;
}

int Conversation::estimateTokens(const QString &text) const
{
    // Rough estimation: ~4 characters per token for English text.
    // For more accuracy, subclasses can use tiktoken or similar.
    if (text.isEmpty())
        return 0;

    // Count words as a proxy (English: ~0.75 tokens per word)
    int words = 0;
    bool inWord = false;
    for (const QChar &ch : text) {
        if (ch.isSpace()) {
            inWord = false;
        } else if (!inWord) {
            ++words;
            inWord = true;
        }
    }

    // Estimate: words * 1.3 ~= tokens (accounts for punctuation, special chars)
    return qMax(1, static_cast<int>(words * 1.3));
}

} // namespace KatHub
