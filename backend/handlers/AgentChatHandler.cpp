#include "AgentChatHandler.h"

#include "HermesCliHelper.h"

#include "httplib.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <QStringList>

AgentChatHandler::AgentChatHandler() = default;

const char* AgentChatHandler::route() { return "/api/agent-chat"; }
IHttpHandler::HttpMethod AgentChatHandler::method() { return HttpMethod::ALL; }

namespace {

struct AgentInfo
{
    QString name;
    QString label;
    QString description;
};

// The Hermes team — matches the profiles KatHub can chat with.
// name = Hermes profile id, label = display name in the UI.
const std::vector<AgentInfo> kAgents = {
    {"orchestrator", "Orchestrator", "Координатор системы: маршрутизация задач, целостность архитектуры"},
    {"analyst",      "Analyst",      "Исследования, анализ данных, отчёты"},
    {"writer",       "Writer",       "Тексты, контент, документы"},
    {"marketer",     "Marketer",     "Продвижение, коммуникации, аудитория"},
    {"coder",        "Coder",        "Техническая реализация, код"},
    {"default",      "Катя",         "Основной ассистент Мишки: KatHub, диалог, vault"},
};

// Extract the session id from `hermes chat -Q` stderr. The CLI prints
// "\nsession_id: <id>" on stderr so stdout stays machine-readable.
QString parseSessionId(const std::string& stderrText)
{
    std::istringstream stream(stderrText);
    std::string line;
    QString last;
    while (std::getline(stream, line)) {
        // trim
        size_t b = line.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) continue;
        size_t e = line.find_last_not_of(" \t\r\n");
        QString trimmed = QString::fromStdString(line.substr(b, e - b + 1));
        if (trimmed.startsWith(QStringLiteral("session_id:"))) {
            last = trimmed.mid(11).trimmed();
        }
    }
    return last;
}

bool isValidAgentName(const QString& name)
{
    return std::any_of(kAgents.begin(), kAgents.end(),
                       [&](const AgentInfo& a) { return a.name == name; });
}

}  // namespace

void AgentChatHandler::handleList(void* response)
{
    auto* res = static_cast<httplib::Response*>(response);
    QJsonArray arr;
    for (const auto& a : kAgents) {
        QJsonObject o;
        o["name"] = a.name;
        o["label"] = a.label;
        o["description"] = a.description;
        arr.append(o);
    }
    QJsonObject root;
    root["agents"] = arr;
    res->set_content(QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString(),
                     "application/json; charset=utf-8");
}

void AgentChatHandler::handleChat(const char* body, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray(body ? body : ""), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        res->status = 400;
        res->set_content("{\"error\":\"invalid JSON body\"}",
                         "application/json; charset=utf-8");
        return;
    }

    QJsonObject obj = doc.object();
    QString agent = obj.value("agent").toString().trimmed();
    QString message = obj.value("message").toString();
    QString sessionId = obj.value("sessionId").toString().trimmed();

    if (!isValidAgentName(agent)) {
        res->status = 400;
        res->set_content("{\"error\":\"unknown agent\"}",
                         "application/json; charset=utf-8");
        return;
    }
    if (message.isEmpty()) {
        res->status = 400;
        res->set_content("{\"error\":\"empty message\"}",
                         "application/json; charset=utf-8");
        return;
    }

    // Serialize one agent at a time — a chat with one profile shouldn't
    // interleave with another.
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> args = {"-p", agent.toStdString(),
                                     "chat", "-q", message.toStdString(), "-Q"};
    if (!sessionId.isEmpty()) {
        args.push_back("-r");
        args.push_back(sessionId.toStdString());
    }

    // Agent replies with tool calls can take minutes; give it a generous budget.
    auto result = HermesCliHelper::runArgv(args, 300000);

    QJsonObject out;
    if (result.exitCode == -2) {
        out["error"] = "timeout";
        out["reply"] = "⚠️ Агент не ответил за 5 минут (таймаут). Попробуйте ещё раз.";
    } else if (result.exitCode != 0) {
        out["error"] = "hermes-exit-" + QString::number(result.exitCode);
        out["reply"] = "⚠️ Ошибка вызова агента (exit " +
                       QString::number(result.exitCode) + ").\n\n" +
                       QString::fromStdString(result.stderrText).trimmed();
    } else {
        QString reply = QString::fromStdString(result.stdoutText).trimmed();
        // Strip stray CLI warnings (e.g. "Warning: Unknown toolsets: messaging")
        // that some profiles emit on stdout before the real reply.
        QStringList lines = reply.split('\n');
        lines.erase(std::remove_if(lines.begin(), lines.end(),
                                   [](const QString& l) {
                                       return l.trimmed().startsWith(QStringLiteral("Warning:"));
                                   }),
                    lines.end());
        reply = lines.join('\n').trimmed();
        if (reply.isEmpty()) {
            reply = QString::fromStdString(result.stderrText).trimmed();
            if (reply.isEmpty()) reply = "(пустой ответ)";
        }
        out["reply"] = reply;
    }

    QString newSession = parseSessionId(result.stderrText);
    if (!newSession.isEmpty()) {
        out["sessionId"] = newSession;
    } else if (!sessionId.isEmpty()) {
        out["sessionId"] = sessionId;  // resume kept the same session
    }

    res->set_content(QJsonDocument(out).toJson(QJsonDocument::Compact).toStdString(),
                     "application/json; charset=utf-8");
}

void AgentChatHandler::handle(const char* request, void* response)
{
    handleWithContext(request, nullptr, nullptr, response, "POST");
}

void AgentChatHandler::handleWithContext(const char* body, const char* path,
                                         const char* query, void* response,
                                         const char* method)
{
    std::string m = method ? method : "POST";
    if (m == "GET") {
        handleList(response);
    } else {
        handleChat(body, response);
    }
    (void)path;
    (void)query;
}
