#pragma once

#include "kathub/ai/IBackendProvider.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class QFileSystemWatcher;
class PluginLoader;

/// Loads AI backend providers from a providers.json file.
///
/// Two config formats are supported (auto-detected per entry):
///
/// Phase 2 format (declarative):
///   {
///     "providers": [
///       {
///         "name": "openai-gpt4",
///         "type": "openai",
///         "endpoint": "https://api.openai.com/v1",
///         "api_key": "${OPENAI_API_KEY}",
///         "model": "gpt-4",
///         "parameters": { "temperature": 0.7, "max_tokens": 4096 }
///       }
///     ]
///   }
///
/// Phase 1 format (DLL-based, backward-compatible):
///   {
///     "providers": [
///       {
///         "name": "test",
///         "dll": "plugins/test_backend.dll",
///         "config": "{\"model\":\"gpt-4\"}"
///       }
///     ]
///   }
///
/// Features:
///   - Env-var resolution in api_key (${VAR_NAME})
///   - Built-in provider factory registry (REGISTER_BACKEND macro)
///   - DLL-based providers via PluginLoader
///   - QFileSystemWatcher-based hot-reload
///   - Hot-reload detection via file mtime (needsReload)

struct ProviderConfig
{
    std::string name;
    std::string type;
    std::string endpoint;
    std::string apiKey;
    std::string apiKeyEnv;
    std::string model;
    std::string dllPath;
    std::string configStr;
    QJsonObject parameters;
    bool isPhase2 = false;
};

using BackendProviderFactory =
    std::function<std::unique_ptr<kathub::ai::IBackendProvider>(
        const ProviderConfig &cfg)>;

class BackendLoader : public QObject
{
    Q_OBJECT

public:
    explicit BackendLoader(PluginLoader &loader,
                           QObject *parent = nullptr);
    ~BackendLoader() override;

    BackendLoader(const BackendLoader &) = delete;
    BackendLoader &operator=(const BackendLoader &) = delete;
    BackendLoader(BackendLoader &&) = delete;
    BackendLoader &operator=(BackendLoader &&) = delete;

    std::vector<kathub::ai::IBackendProvider *> loadFromFile(
        const std::string &path);
    std::vector<kathub::ai::IBackendProvider *> reload();

    bool needsReload(const std::string &path) const;
    void enableHotReload(bool enable = true);
    bool isHotReloadEnabled() const;

    std::string lastError() const { return lastError_; }
    const std::vector<kathub::ai::IBackendProvider *> &providers() const
    {
        return providers_;
    }
    bool isLoaded() const { return isLoaded_; }
    std::string filePath() const { return filePath_; }

    kathub::ai::IBackendProvider *provider(const std::string &name) const;
    size_t providerCount() const { return providers_.size(); }

    static void registerFactory(const std::string &type,
                                BackendProviderFactory factory);

signals:
    void providersChanged();
    void providerLoadFailed(const QString &name, const QString &error);

private:
    static std::string resolveEnvVar(const std::string &raw);
    std::optional<ProviderConfig>
    parseProviderEntry(const QJsonObject &obj, int index);
    kathub::ai::IBackendProvider *instantiateProvider(const ProviderConfig &cfg);
    kathub::ai::IBackendProvider *
    instantiateFromDll(const ProviderConfig &cfg);
    kathub::ai::IBackendProvider *
    instantiateFromFactory(const ProviderConfig &cfg);
    void unloadAll();
    std::vector<kathub::ai::IBackendProvider *>
    loadImpl(const std::string &path);

    PluginLoader &loader_;
    std::vector<kathub::ai::IBackendProvider *> providers_;
    std::string lastError_;
    std::string filePath_;
    bool isLoaded_ = false;
    mutable std::filesystem::file_time_type lastWriteTime_;
    bool hasLastWriteTime_ = false;
    std::unique_ptr<QFileSystemWatcher> fileWatcher_;

    static std::unordered_map<std::string, BackendProviderFactory> &
    factories();
};

#define REGISTER_BACKEND(Type, ClassName)                                      \
    namespace                                                                  \
    {                                                                          \
        static const bool _kathub_backend_##__COUNTER__ = []() -> bool {       \
            BackendLoader::registerFactory(                                    \
                Type,                                                          \
                [](const ProviderConfig &cfg)                                  \
                    -> std::unique_ptr<kathub::ai::IBackendProvider> {         \
                    auto p = std::make_unique<ClassName>();                    \
                    QJsonObject configJson;                                    \
                    configJson[QStringLiteral("endpoint")] =                   \
                        QString::fromStdString(cfg.endpoint);                  \
                    configJson[QStringLiteral("api_key")] =                    \
                        QString::fromStdString(cfg.apiKey);                    \
                    configJson[QStringLiteral("model")] =                      \
                        QString::fromStdString(cfg.model);                     \
                    configJson[QStringLiteral("parameters")] =                 \
                        cfg.parameters;                                        \
                    QJsonDocument doc(configJson);                             \
                    if (!p->initialize(doc.toJson(QJsonDocument::Compact)      \
                                           .toStdString())) {                  \
                        return nullptr;                                        \
                    }                                                          \
                    return p;                                                  \
                });                                                            \
            return true;                                                       \
        }();                                                                   \
    }
