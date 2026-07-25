#include "AIService.h"
#include "OpenRouterClient.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <iostream>
#include <sstream>

namespace KatHub {

// ============================================================================
//  Construction
// ============================================================================

AIService::AIService(QObject *parent)
    : QObject(parent)
{
}

AIService::~AIService() = default;

// ============================================================================
//  Provider management
// ============================================================================

void AIService::addProvider(kathub::ai::IBackendProvider *provider)
{
    if (!provider)
        return;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Avoid duplicates by pointer.
    for (const auto &e : m_providers) {
        if (e.provider == provider)
            return;
    }

    Entry entry;
    entry.provider = provider;
    entry.name     = provider->getName();

    m_providers.push_back(entry);

    std::cout << "AIService: added provider \"" << entry.name << "\""
              << " (total: " << m_providers.size() << ")" << std::endl;

    emit providerAdded(QString::fromStdString(entry.name));
}

void AIService::addProvider(std::unique_ptr<kathub::ai::IBackendProvider> provider)
{
    if (!provider)
        return;

    kathub::ai::IBackendProvider *raw = provider.get();
    m_ownedProviders.push_back(std::move(provider));
    addProvider(raw);
}

bool AIService::addOpenRouterProvider(const std::string &name,
                                      const std::string &apiKey,
                                      const std::string &model,
                                      const std::string &endpoint)
{
    // Build JSON config
    QJsonObject cfg;
    cfg[QStringLiteral("name")]     = QString::fromStdString(name);
    cfg[QStringLiteral("api_key")]  = QString::fromStdString(apiKey);
    cfg[QStringLiteral("model")]    = QString::fromStdString(model);
    cfg[QStringLiteral("endpoint")] = QString::fromStdString(endpoint);

    QJsonDocument doc(cfg);
    std::string configJson = doc.toJson(QJsonDocument::Compact).toStdString();

    auto client = std::make_unique<OpenRouterClient>();
    if (!client->initialize(configJson)) {
        std::cerr << "AIService: failed to initialize OpenRouter provider \""
                  << name << "\"" << std::endl;
        return false;
    }

    addProvider(std::move(client));
    return true;
}

void AIService::removeProvider(kathub::ai::IBackendProvider *provider)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_providers.begin(), m_providers.end(),
        [provider](const Entry &e) { return e.provider == provider; });

    if (it != m_providers.end()) {
        QString name = QString::fromStdString(it->name);
        m_providers.erase(it);
        emit providerRemoved(name);
    }
}

void AIService::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_providers.clear();
    m_roundRobinIndex = 0;
}

size_t AIService::providerCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_providers.size();
}

std::vector<std::string> AIService::providerNames() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_providers.size());
    for (const auto &e : m_providers)
        names.push_back(e.name);
    return names;
}

// ============================================================================
//  Dispatch strategy
// ============================================================================

void AIService::setStrategy(Strategy strategy)
{
    m_strategy = strategy;
}

AIService::Strategy AIService::strategy() const
{
    return m_strategy;
}

// ============================================================================
//  chat() — dispatch via current strategy
// ============================================================================

std::string AIService::chat(const std::string &prompt)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_providers.empty())
        return "ERROR: No AI providers registered";

    kathub::ai::IBackendProvider *provider = nullptr;

    switch (m_strategy) {
    case Strategy::RoundRobin: {
        size_t idx = m_roundRobinIndex.fetch_add(1) % m_providers.size();
        provider = m_providers[idx].provider;
        break;
    }
    case Strategy::PickFirst: {
        provider = m_providers.front().provider;
        break;
    }
    }

    if (!provider)
        return "ERROR: Selected provider is null";

    try {
        return provider->chat(prompt);
    } catch (const std::exception &e) {
        std::ostringstream oss;
        oss << "ERROR: chat() threw: " << e.what();
        return oss.str();
    } catch (...) {
        return "ERROR: chat() threw unknown exception";
    }
}

// ============================================================================
//  chatTo() — dispatch to specific named provider
// ============================================================================

std::string AIService::chatTo(const std::string &providerName,
                              const std::string &prompt)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto &e : m_providers) {
        if (e.name == providerName) {
            try {
                return e.provider->chat(prompt);
            } catch (const std::exception &ex) {
                std::ostringstream oss;
                oss << "ERROR: chat() threw: " << ex.what();
                return oss.str();
            } catch (...) {
                return "ERROR: chat() threw unknown exception";
            }
        }
    }

    return "ERROR: Provider not found: " + providerName;
}

// ============================================================================
//  Status
// ============================================================================

bool AIService::isAvailable() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_providers.empty();
}

} // namespace KatHub
