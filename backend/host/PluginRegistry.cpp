#include "PluginRegistry.h"
#include "IPlugin.h"
#include "IHttpHandler.h"
#include "SignalHub.h"

#include <QJsonObject>

// ============================================================================
//  PluginRegistry
// ============================================================================

PluginRegistry &PluginRegistry::instance()
{
    static PluginRegistry reg;
    return reg;
}

void PluginRegistry::setSignalHub(KatHub::SignalHub *hub)
{
    signalHub_ = hub;
}

void PluginRegistry::registerPlugin(IPlugin *plugin)
{
    if (!plugin) return;
    std::unique_lock lock(mutex_);
    plugins_[plugin->name()] = plugin;

    if (signalHub_) {
        QJsonObject payload;
        payload[QStringLiteral("name")] = QString::fromStdString(plugin->name());
        signalHub_->publish(QStringLiteral("plugin.registered"), payload);
    }
}

void PluginRegistry::unregisterPlugin(const std::string &name)
{
    std::unique_lock lock(mutex_);
    plugins_.erase(name);

    if (signalHub_) {
        QJsonObject payload;
        payload[QStringLiteral("name")] = QString::fromStdString(name);
        signalHub_->publish(QStringLiteral("plugin.unregistered"), payload);
    }
}

IPlugin *PluginRegistry::findByName(const std::string &name) const
{
    std::shared_lock lock(mutex_);
    auto it = plugins_.find(name);
    return (it != plugins_.end()) ? it->second : nullptr;
}

std::vector<IPlugin *> PluginRegistry::allPlugins() const
{
    std::shared_lock lock(mutex_);
    std::vector<IPlugin *> result;
    result.reserve(plugins_.size());
    for (const auto &pair : plugins_)
        result.push_back(pair.second);
    return result;
}

// ---- HTTP handlers ----

void PluginRegistry::registerHandler(IHttpHandler *handler)
{
    if (!handler) return;
    std::unique_lock lock(mutex_);
    httpHandlers_.push_back(handler);

    if (signalHub_) {
        QJsonObject payload;
        payload[QStringLiteral("route")] = QString::fromLatin1(handler->route());
        payload[QStringLiteral("method")] = QString::fromLatin1(
            handler->method() == IHttpHandler::HttpMethod::GET    ? "GET" :
            handler->method() == IHttpHandler::HttpMethod::POST   ? "POST" :
            handler->method() == IHttpHandler::HttpMethod::PUT    ? "PUT" :
            handler->method() == IHttpHandler::HttpMethod::DELETE ? "DELETE" : "GET");
        signalHub_->publish(QStringLiteral("handler.registered"), payload);
    }
}

std::vector<IHttpHandler *> PluginRegistry::handlers() const
{
    std::shared_lock lock(mutex_);
    return httpHandlers_;
}

// ---- Draining ----

void PluginRegistry::markDraining(const std::string &name)
{
    std::unique_lock lock(mutex_);
    draining_.insert(name);
}

bool PluginRegistry::isDraining(const std::string &name) const
{
    std::shared_lock lock(mutex_);
    return draining_.count(name) > 0;
}

void PluginRegistry::clearDraining(const std::string &name)
{
    std::unique_lock lock(mutex_);
    draining_.erase(name);
}

// ============================================================================
//  StaticHandlerRegistry
// ============================================================================

StaticHandlerRegistry &StaticHandlerRegistry::instance()
{
    static StaticHandlerRegistry reg;
    return reg;
}

void StaticHandlerRegistry::registerHandler(IHttpHandler *handler)
{
    if (handler)
        handlers_.push_back(handler);
}

const std::vector<IHttpHandler *> &StaticHandlerRegistry::handlers() const
{
    return handlers_;
}
