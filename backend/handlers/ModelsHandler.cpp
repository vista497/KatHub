#include "ModelsHandler.h"
#include "HermesCliHelper.h"

#include "httplib.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QFile>

#include <sstream>

ModelsHandler::ModelsHandler() = default;

const char* ModelsHandler::route() { return "/api/models"; }
IHttpHandler::HttpMethod ModelsHandler::method() { return HttpMethod::ALL; }

static std::string loadModelsList()
{
    // Get current model from `hermes model` (interactive, so parse first line)
    std::string raw = HermesCliHelper::run("model");
    
    QJsonObject result;
    
    // Parse "Current model:    deepseek/deepseek-v4-pro"
    std::string currentModel;
    std::istringstream stream(raw);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.find("Current model:") != std::string::npos) {
            size_t pos = line.find("Current model:");
            if (pos != std::string::npos) {
                currentModel = line.substr(pos + 14); // len of "Current model:"
                // Trim
                while (!currentModel.empty() && currentModel.front() == ' ')
                    currentModel.erase(0, 1);
                while (!currentModel.empty() && currentModel.back() == ' ')
                    currentModel.pop_back();
            }
            break;
        }
    }

    // Also try config
    QJsonArray models;
    result["current"] = QString::fromStdString(currentModel);

    // Read available models from config
    std::string configPath = HermesCliHelper::run("config path");
    // Fallback: known providers from `hermes model` interactive list
    std::vector<std::string> providers = {
        "deepseek/deepseek-v4-pro",
        "anthropic/claude-sonnet-4",
        "openai/gpt-4o",
        "openrouter/anthropic/claude-sonnet-4"
    };
    
    for (const auto& m : providers) {
        QJsonObject entry;
        entry["name"] = QString::fromStdString(m);
        entry["provider"] = QString::fromStdString(m.substr(0, m.find('/')));
        entry["available"] = true;
        models.append(entry);
    }

    result["models"] = models;
    return QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString();
}

void ModelsHandler::handle(const char*, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);
    res->set_content(loadModelsList(), "application/json; charset=utf-8");
}

void ModelsHandler::handleWithContext(
    const char*, const char*, const char*, void* response, const char*)
{
    auto* res = static_cast<httplib::Response*>(response);
    res->set_content(loadModelsList(), "application/json; charset=utf-8");
}
