#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QString>
#include <functional>
#include <memory>

namespace KatHub {

class SignalHub;

/// Dispatches AI tool calls to registered handlers.
///
/// Tools are registered by name with a handler function.
/// Tool definitions can be exported for inclusion in AI prompts.
/// Parses tool calls from AI response text (JSON blocks).
class ToolDispatcher : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ToolDispatcher)

public:
    /// Handler signature: receives tool arguments, returns result JSON.
    /// Result should contain at minimum {"success": true/false, "output": "..."}
    using ToolHandler = std::function<QJsonObject(const QJsonObject &args)>;

    explicit ToolDispatcher(QObject *parent = nullptr);
    ~ToolDispatcher() override;

    /// Set the SignalHub for publishing tool-call events.
    void setSignalHub(SignalHub *hub);

    /// Register a tool with handler.
    /// @param name         Unique tool name (e.g. "read_file")
    /// @param description  Human-readable description for AI context
    /// @param handler      Function to execute when tool is called
    void registerTool(const QString &name,
                      const QString &description,
                      ToolHandler handler);

    /// Unregister a previously registered tool.
    void unregisterTool(const QString &name);

    /// Dispatch a tool call by name with arguments.
    /// Returns the handler's result or an error JSON.
    QJsonObject dispatch(const QString &name, const QJsonObject &args);

    /// Parse tool calls from an AI response text and dispatch them.
    /// Looks for JSON blocks with "tool" or "function" fields.
    /// Returns array of results.
    QJsonArray parseAndDispatch(const QString &responseText);

    /// Get all registered tool definitions (name + description)
    /// suitable for inclusion in a system prompt.
    QJsonArray getToolDefinitions() const;

    /// Check if a tool is registered.
    bool hasTool(const QString &name) const;

    /// Number of registered tools.
    int toolCount() const;

signals:
    /// Emitted when a tool is called (before handler executes).
    void toolCalled(const QString &name, const QJsonObject &args);

    /// Emitted when a tool completes successfully.
    void toolResult(const QString &name, const QJsonObject &result);

    /// Emitted when a tool call fails.
    void toolError(const QString &name, const QString &error);

private:
    struct ToolDef {
        QString     name;
        QString     description;
        ToolHandler handler;
    };

    QJsonArray extractToolCalls(const QString &text) const;

    QHash<QString, ToolDef> m_tools;
    SignalHub              *m_signalHub = nullptr;
};

} // namespace KatHub
