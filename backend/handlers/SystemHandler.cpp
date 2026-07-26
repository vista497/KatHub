#include "SystemHandler.h"

#include "httplib.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QByteArray>

#include <sstream>
#include <string>

SystemHandler::SystemHandler() = default;

const char* SystemHandler::route()
{
    return "/api/system";
}

IHttpHandler::HttpMethod SystemHandler::method()
{
    return HttpMethod::GET;
}

void SystemHandler::setApiClient(std::shared_ptr<HermesApiClient> client)
{
    api_ = std::move(client);
}

void SystemHandler::setStartTime(std::chrono::steady_clock::time_point t)
{
    startTime_ = t;
}

void SystemHandler::setPorts(int httpPort, int wsPort)
{
    httpPort_ = httpPort;
    wsPort_  = wsPort;
}

void SystemHandler::handle(const char*, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);

    // Calculate uptime in seconds.
    long long uptimeSec = 0;
    if (startTime_ != std::chrono::steady_clock::time_point{}) {
        auto now = std::chrono::steady_clock::now();
        uptimeSec = std::chrono::duration_cast<std::chrono::seconds>(
                        now - startTime_).count();
    }

    // Fetch Hermes system status if API client is available.
    bool hermesAlive = false;
    std::string currentModel = "unknown";
    std::string hermesVersion = "unknown";

    if (api_) {
        hermesAlive = api_->isAlive();
        std::string statusJson = api_->getSystemStatus();
        if (!statusJson.empty()) {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(
                QByteArray::fromStdString(statusJson), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("model")) {
                    currentModel = obj["model"].toString().toStdString();
                }
                if (obj.contains("version")) {
                    hermesVersion = obj["version"].toString().toStdString();
                }
            }
        }

        // Also grab current model from Hermes models endpoint.
        std::string modelJson = api_->getModel("current");
        if (!modelJson.empty()) {
            QJsonParseError err2;
            QJsonDocument mdoc = QJsonDocument::fromJson(
                QByteArray::fromStdString(modelJson), &err2);
            if (err2.error == QJsonParseError::NoError && mdoc.isObject()) {
                QJsonObject mobj = mdoc.object();
                // Model name may be nested: {"model": "..."} or {"name": "..."} or {"id": "..."}
                if (mobj.contains("model")) {
                    currentModel = mobj["model"].toString().toStdString();
                } else if (mobj.contains("name")) {
                    currentModel = mobj["name"].toString().toStdString();
                } else if (mobj.contains("id")) {
                    currentModel = mobj["id"].toString().toStdString();
                }
            }
        }
    }

    // Build aggregated response.
    std::ostringstream json;
    json << "{";
    json << R"("status":"ok")";
    json << R"(,"version":"0.1.0")";
    json << ",\"uptime\":" << uptimeSec;
    json << ",\"httpPort\":" << httpPort_;
    json << ",\"wsPort\":" << wsPort_;
    json << ",\"hermes\":{";
    json << R"("alive":)" << (hermesAlive ? "true" : "false");
    json << R"(,"url":"http://127.0.0.1:8642")";
    json << R"(,"version":")" << hermesVersion << "\"";
    if (!currentModel.empty() && currentModel != "unknown") {
        json << R"(,"model":")" << currentModel << "\"";
    }
    json << "}";
    json << "}";

    res->set_content(json.str(), "application/json; charset=utf-8");
}
