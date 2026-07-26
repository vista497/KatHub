#include "ToolDispatcher.h"
#include "core/SignalHub.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QDebug>

namespace KatHub {

// ============================================================================
// Construction
// ============================================================================

ToolDispatcher::ToolDispatcher(QObject *parent)
    : QObject(parent)
{
}

ToolDispatcher::~ToolDispatcher() = default;

// ============================================================================
// SignalHub
// ============================================================================

void ToolDispatcher::setSignalHub(SignalHub *hub)
{
    m_signalHub = hub;
}

// ============================================================================
// registerTool
// ============================================================================

void ToolDispatcher::registerTool(const QString &name,
                                   const QString &description,
                                   ToolHandler handler)
{
    if (name.isEmpty()) {
        qWarning() << "[ToolDispatcher] Cannot register tool with empty name";
        return;
    }

    if (!handler) {
        qWarning() << "[ToolDispatcher] Cannot register tool" << name
                    << "with null handler";
        return;
    }

    ToolDef def;
    def.name        = name;
    def.description = description;
    def.handler     = std::move(handler);

    m_tools[name] = std::move(def);

    qDebug() << "[ToolDispatcher] Registered tool:" << name;
}

// ============================================================================
// unregisterTool
// ============================================================================

void ToolDispatcher::unregisterTool(const QString &name)
{
    if (m_tools.remove(name)) {
        qDebug() << "[ToolDispatcher] Unregistered tool:" << name;
    }
}

// ============================================================================
// dispatch
// ============================================================================

QJsonObject ToolDispatcher::dispatch(const QString &name, const QJsonObject &args)
{
    auto it = m_tools.find(name);
    if (it == m_tools.end()) {
        QString err = QStringLiteral("Tool not found: %1").arg(name);
        qWarning() << "[ToolDispatcher]" << err;

        emit toolError(name, err);

        QJsonObject result;
        result[QStringLiteral("success")] = false;
        result[QStringLiteral("error")]   = err;
        return result;
    }

    emit toolCalled(name, args);

    // Publish to SignalHub if available
    if (m_signalHub) {
        QJsonObject eventData;
        eventData[QStringLiteral("tool")] = name;
        eventData[QStringLiteral("args")] = args;
        m_signalHub->publish(QStringLiteral("ai:tool:called"), eventData);
    }

    try {
        QJsonObject result = it->handler(args);

        // Ensure success field
        if (!result.contains(QStringLiteral("success"))) {
            result[QStringLiteral("success")] = true;
        }

        emit toolResult(name, result);

        if (m_signalHub) {
            QJsonObject eventData;
            eventData[QStringLiteral("tool")]   = name;
            eventData[QStringLiteral("result")] = result;
            m_signalHub->publish(QStringLiteral("ai:tool:result"), eventData);
        }

        return result;
    } catch (const std::exception &e) {
        QString err = QStringLiteral("Tool %1 threw: %2").arg(name, e.what());
        qWarning() << "[ToolDispatcher]" << err;

        emit toolError(name, err);

        QJsonObject result;
        result[QStringLiteral("success")] = false;
        result[QStringLiteral("error")]   = err;
        return result;
    } catch (...) {
        QString err = QStringLiteral("Tool %1 threw unknown exception").arg(name);
        qWarning() << "[ToolDispatcher]" << err;

        emit toolError(name, err);

        QJsonObject result;
        result[QStringLiteral("success")] = false;
        result[QStringLiteral("error")]   = err;
        return result;
    }
}

// ============================================================================
// parseAndDispatch
// ============================================================================

QJsonArray ToolDispatcher::parseAndDispatch(const QString &responseText)
{
    QJsonArray results;

    QJsonArray toolCalls = extractToolCalls(responseText);
    if (toolCalls.isEmpty())
        return results;

    qDebug() << "[ToolDispatcher] Found" << toolCalls.size()
             << "tool calls in response";

    for (const auto &val : toolCalls) {
        QJsonObject call = val.toObject();

        QString name = call[QStringLiteral("name")].toString();
        if (name.isEmpty())
            name = call[QStringLiteral("tool")].toString();
        if (name.isEmpty())
            name = call[QStringLiteral("function")].toString();

        QJsonObject args = call[QStringLiteral("arguments")].toObject();
        if (args.isEmpty())
            args = call[QStringLiteral("args")].toObject();
        if (args.isEmpty())
            args = call[QStringLiteral("parameters")].toObject();

        if (name.isEmpty()) {
            qWarning() << "[ToolDispatcher] Tool call has no name field";
            continue;
        }

        QJsonObject result = dispatch(name, args);
        result[QStringLiteral("_tool_name")] = name;
        results.append(result);
    }

    return results;
}

// ============================================================================
// getToolDefinitions
// ============================================================================

QJsonArray ToolDispatcher::getToolDefinitions() const
{
    QJsonArray defs;
    for (const auto &tool : m_tools) {
        QJsonObject def;
        def[QStringLiteral("name")]        = tool.name;
        def[QStringLiteral("description")] = tool.description;
        defs.append(def);
    }
    return defs;
}

// ============================================================================
// hasTool / toolCount
// ============================================================================

bool ToolDispatcher::hasTool(const QString &name) const
{
    return m_tools.contains(name);
}

int ToolDispatcher::toolCount() const
{
    return m_tools.size();
}

// ============================================================================
// extractToolCalls — parse tool calls from AI response text
// ============================================================================

QJsonArray ToolDispatcher::extractToolCalls(const QString &text) const
{
    QJsonArray calls;

    // Strategy 1: Look for ```tool_call or ```json code blocks containing tool calls
    static QRegularExpression codeBlock(
        QStringLiteral("```(?:tool_call|json)?\\s*\\n?"
                       "([\\s\\S]*?)"
                       "```"),
        QRegularExpression::MultilineOption
    );

    auto it = codeBlock.globalMatch(text);
    while (it.hasNext()) {
        auto match = it.next();
        QString blockContent = match.captured(1).trimmed();

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(blockContent.toUtf8(), &err);

        if (err.error == QJsonParseError::NoError) {
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                // Check if this looks like a tool call
                if (obj.contains(QStringLiteral("name")) ||
                    obj.contains(QStringLiteral("tool")) ||
                    obj.contains(QStringLiteral("function"))) {
                    calls.append(obj);
                }
            } else if (doc.isArray()) {
                QJsonArray arr = doc.array();
                for (const auto &val : arr) {
                    QJsonObject obj = val.toObject();
                    if (obj.contains(QStringLiteral("name")) ||
                        obj.contains(QStringLiteral("tool")) ||
                        obj.contains(QStringLiteral("function"))) {
                        calls.append(obj);
                    }
                }
            }
        }
    }

    // Strategy 2: Look for inline JSON objects with tool/function fields
    if (calls.isEmpty()) {
        static QRegularExpression jsonObj(
            QStringLiteral("\\{[^{}]*\"(?:name|tool|function)\"[^{}]*\\}"),
            QRegularExpression::MultilineOption
        );

        auto it2 = jsonObj.globalMatch(text);
        while (it2.hasNext()) {
            auto match = it2.next();
            QString jsonStr = match.captured(0);

            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &err);

            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains(QStringLiteral("name")) ||
                    obj.contains(QStringLiteral("tool")) ||
                    obj.contains(QStringLiteral("function"))) {
                    calls.append(obj);
                }
            }
        }
    }

    return calls;
}

} // namespace KatHub
