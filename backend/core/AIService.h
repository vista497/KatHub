#pragma once

#include <kathub/ai/IBackendProvider.h>

#include <QObject>
#include <QString>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace KatHub {

/// High-level AI service that manages a list of IBackendProvider instances.
///
/// Supports dispatch strategies:
///   - RoundRobin — cycles through providers
///   - PickFirst  — always uses the first available provider
///
/// Thread-safe for concurrent chat() calls from httplib worker threads.
class AIService : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AIService)

public:
    enum class Strategy
    {
        RoundRobin,
        PickFirst
    };

    explicit AIService(QObject *parent = nullptr);
    ~AIService() override;

    // ---- Provider management ----

    /// Add a provider. AIService does NOT take ownership.
    void addProvider(kathub::ai::IBackendProvider *provider);

    /// Remove a provider by pointer.
    void removeProvider(kathub::ai::IBackendProvider *provider);

    /// Remove all providers.
    void clear();

    /// Number of registered providers.
    size_t providerCount() const;

    /// Get all registered provider names.
    std::vector<std::string> providerNames() const;

    // ---- Dispatch ----

    /// Set the dispatch strategy.
    void setStrategy(Strategy strategy);

    /// Current dispatch strategy.
    Strategy strategy() const;

    /// Send a chat prompt using the current strategy.
    /// Returns the model response or an error string prefixed with "ERROR:".
    std::string chat(const std::string &prompt);

    /// Send a chat prompt to a specific named provider.
    /// Returns empty string if the provider is not found.
    std::string chatTo(const std::string &providerName,
                       const std::string &prompt);

    // ---- Status ----

    /// Returns true if at least one provider is registered.
    bool isAvailable() const;

signals:
    void providerAdded(const QString &name);
    void providerRemoved(const QString &name);

private:
    struct Entry
    {
        kathub::ai::IBackendProvider *provider = nullptr;
        std::string name;
    };

    mutable std::mutex m_mutex;
    std::vector<Entry> m_providers;
    std::atomic<size_t> m_roundRobinIndex{0};
    Strategy m_strategy = Strategy::RoundRobin;
};

} // namespace KatHub
