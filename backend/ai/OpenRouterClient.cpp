#include "OpenRouterClient.h"

#include "httplib.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <iostream>
#include <sstream>

namespace KatHub {

OpenRouterClient::OpenRouterClient() = default;

OpenRouterClient::~OpenRouterClient()
{
    // httplib Client is auto-destroyed via unique_ptr
}

// ============================================================================
//  initialize
// ============================================================================

bool OpenRouterClient::initialize(const std::string &config)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromStdString(config), &err);

    if (err.error != QJsonParseError::NoError) {
        std::cerr << "[OpenRouterClient] JSON parse error: "
                  << err.errorString().toStdString() << std::endl;
        return false;
    }

    if (!doc.isObject()) {
        std::cerr << "[OpenRouterClient] Config must be a JSON object"
                  << std::endl;
        return false;
    }

    QJsonObject root = doc.object();

    m_endpoint = root.value(QStringLiteral("endpoint")).toString().toStdString();
    m_apiKey   = root.value(QStringLiteral("api_key")).toString().toStdString();
    m_model    = root.value(QStringLiteral("model")).toString().toStdString();
    m_name     = root.value(QStringLiteral("name")).toString("OpenRouter").toStdString();

    if (m_endpoint.empty()) {
        m_endpoint = "https://openrouter.ai/api/v1";
    }

    if (m_apiKey.empty()) {
        std::cerr << "[OpenRouterClient] Missing api_key in config"
                  << std::endl;
        return false;
    }

    if (m_model.empty()) {
        m_model = "openai/gpt-4o";
    }

    QJsonObject params = root.value(QStringLiteral("parameters")).toObject();
    if (params.contains(QStringLiteral("temperature"))) {
        m_temperature = params.value(QStringLiteral("temperature")).toDouble(0.7);
    }
    if (params.contains(QStringLiteral("max_tokens"))) {
        m_maxTokens = params.value(QStringLiteral("max_tokens")).toInt(4096);
    }

    // Extract scheme+host for httplib Client (universal constructor)
    std::string schemeHost;
    {
        std::string url = m_endpoint;
        if (!url.empty() && url.back() == '/')
            url.pop_back();

        size_t schemeEnd = url.find("://");
        if (schemeEnd != std::string::npos) {
            std::string scheme = url.substr(0, schemeEnd);
            url = url.substr(schemeEnd + 3);
            size_t pathStart = url.find('/');
            std::string host;
            if (pathStart != std::string::npos) {
                host = url.substr(0, pathStart);
            } else {
                host = url;
            }
            schemeHost = scheme + "://" + host;
        } else {
            schemeHost = "https://" + url;
        }
    }

    m_client = std::make_unique<httplib::Client>(schemeHost);

    // Set auth and default headers (NO Content-Type — httplib sets it per-request)
    m_client->set_bearer_token_auth(m_apiKey);
    m_client->set_default_headers({
        {"HTTP-Referer", "https://github.com/nousresearch/KatHub"},
        {"X-Title", "KatHub"},
    });

    m_initialized = true;

    std::cout << "[OpenRouterClient] Initialized: endpoint=" << m_endpoint
              << " model=" << m_model << " name=" << m_name << std::endl;

    return true;
}

// ============================================================================
//  chat
// ============================================================================

std::string OpenRouterClient::chat(const std::string &prompt)
{
    if (!m_initialized || !m_client) {
        return "ERROR: OpenRouterClient not initialized";
    }

    // Build request body
    QJsonObject body;
    body[QStringLiteral("model")] = QString::fromStdString(m_model);

    QJsonArray messages;
    QJsonObject userMsg;
    userMsg[QStringLiteral("role")] = QStringLiteral("user");
    userMsg[QStringLiteral("content")] = QString::fromStdString(prompt);
    messages.append(userMsg);
    body[QStringLiteral("messages")] = messages;

    body[QStringLiteral("temperature")] = m_temperature;
    body[QStringLiteral("max_tokens")] = m_maxTokens;

    QJsonDocument reqDoc(body);
    std::string reqBody = reqDoc.toJson(QJsonDocument::Compact).toStdString();

    // Build chat completions path
    std::string chatPath;
    {
        std::string url = m_endpoint;
        if (!url.empty() && url.back() == '/')
            url.pop_back();

        size_t schemeEnd = url.find("://");
        if (schemeEnd != std::string::npos) {
            url = url.substr(schemeEnd + 3);
        }

        size_t pathStart = url.find('/');
        if (pathStart != std::string::npos) {
            chatPath = url.substr(pathStart) + "/chat/completions";
        } else {
            chatPath = "/chat/completions";
        }
        // Normalize double slashes
        while (chatPath.find("//") != std::string::npos) {
            chatPath.replace(chatPath.find("//"), 2, "/");
        }
    }

    std::cout << "[OpenRouterClient] POST " << chatPath
              << " prompt_len=" << prompt.size() << std::endl;

    auto res = m_client->Post(chatPath, reqBody, "application/json");

    if (!res) {
        return "ERROR: HTTP request failed (connection error or timeout)";
    }

    if (res->status != 200) {
        std::ostringstream oss;
        oss << "ERROR: HTTP " << res->status << ": " << res->body;
        return oss.str();
    }

    // Parse response
    QJsonParseError parseErr;
    QJsonDocument respDoc = QJsonDocument::fromJson(
        QByteArray::fromStdString(res->body), &parseErr);

    if (parseErr.error != QJsonParseError::NoError) {
        std::ostringstream oss;
        oss << "ERROR: Failed to parse response JSON: "
            << parseErr.errorString().toStdString();
        return oss.str();
    }

    QJsonObject respObj = respDoc.object();

    // Try choices[0].message.content
    QJsonArray choices = respObj.value(QStringLiteral("choices")).toArray();
    if (!choices.isEmpty()) {
        QJsonObject first = choices[0].toObject();
        QJsonObject message = first.value(QStringLiteral("message")).toObject();
        QString content = message.value(QStringLiteral("content")).toString();
        if (!content.isEmpty()) {
            return content.toStdString();
        }
    }

    // Fallback: error field
    QJsonObject errorObj = respObj.value(QStringLiteral("error")).toObject();
    if (!errorObj.isEmpty()) {
        std::ostringstream oss;
        oss << "ERROR: "
            << errorObj.value(QStringLiteral("message")).toString().toStdString();
        return oss.str();
    }

    return "ERROR: Unexpected response format";
}

// ============================================================================
//  getCapabilities
// ============================================================================

std::string OpenRouterClient::getCapabilities()
{
    return "chat,text-completion";
}

// ============================================================================
//  getName
// ============================================================================

std::string OpenRouterClient::getName()
{
    return m_name.empty() ? "OpenRouter" : m_name;
}

} // namespace KatHub
