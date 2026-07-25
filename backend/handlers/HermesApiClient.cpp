#include "HermesApiClient.h"

#include "httplib.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

HermesApiClient::HermesApiClient(const std::string& baseUrl, const std::string& apiKey)
    : baseUrl_(baseUrl), apiKey_(apiKey) {}

std::string HermesApiClient::request(const std::string& method,
                                      const std::string& path,
                                      const std::string& body)
{
    httplib::Client cli(baseUrl_);
    cli.set_connection_timeout(10);
    cli.set_read_timeout(60);

    httplib::Headers headers = {
        {"Authorization", "Bearer " + apiKey_},
        {"Content-Type", "application/json"}
    };

    httplib::Result res;
    if (method == "GET")
        res = cli.Get(path, headers);
    else if (method == "POST")
        res = cli.Post(path, headers, body, "application/json");
    else if (method == "DELETE")
        res = cli.Delete(path, headers);
    else
        return R"({"error":"Unsupported method"})";

    if (!res) {
        QJsonObject err;
        err["error"] = QStringLiteral("Hermes API unreachable: %1")
            .arg(httplib::to_string(res.error()).c_str());
        return QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString();
    }

    if (res->status >= 400) {
        QJsonObject err;
        err["error"] = QStringLiteral("Hermes API error %1: %2")
            .arg(res->status)
            .arg(QString::fromStdString(res->body).left(200));
        return QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString();
    }

    return res->body;
}

bool HermesApiClient::isAlive()
{
    httplib::Client cli(baseUrl_);
    cli.set_connection_timeout(3);
    auto res = cli.Get("/health");
    return res && res->status == 200;
}

std::string HermesApiClient::listSessions()
{
    return request("GET", "/api/sessions");
}

std::string HermesApiClient::getMessages(const std::string& sessionId)
{
    return request("GET", "/api/sessions/" + sessionId + "/messages");
}

std::string HermesApiClient::createSession()
{
    return request("POST", "/api/sessions", "{}");
}

std::string HermesApiClient::chat(const std::string& sessionId, const std::string& message)
{
    QJsonObject body;
    body["message"] = QString::fromStdString(message);
    QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return request("POST", "/api/sessions/" + sessionId + "/chat", json.toStdString());
}

std::string HermesApiClient::deleteSession(const std::string& sessionId)
{
    return request("DELETE", "/api/sessions/" + sessionId);
}
