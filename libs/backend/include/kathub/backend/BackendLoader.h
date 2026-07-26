#pragma once

#include <kathub/ai/IBackendProvider.h>

#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QObject>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class PluginLoader;

namespace kathub {
namespace backend {

// ---------------------------------------------------------------------------
// ProviderConfig — parsed config entry for a single AI provider
// ---------------------------------------------------------------------------
struct ProviderConfig
{
    std::string name;         // unique provider name (e.g. "openai-gpt4")
    std::string type;         // provider type (e.g. "openai", "ollama")
    std::string endpoint;     // API endpoint URL
    std::string apiKey;       // resolved API key (env vars expanded)
    std::string apiKeyEnv;    // original env-var ref (e.g. "${OPENAI_API_KEY}")
    std::string model;        // model name (e.g. "gpt-4")
    QJsonObject parameters;   // extra parameters (temperature, max_tokens, …)
};

// ---------------------------------------------------------------------------
// BackendProviderFactory — creates an IBackendProvider from config
// ---------------------------------------------------------------------------
using BackendProviderFactory =
    std::function<std::unique_ptr<kathub::ai::IBackendProvider>(
        const ProviderConfig &cfg)>;

// ---------------------------------------------------------------------------
// BackendLoader
// ---------------------------------------------------------------------------
/// Reads providers.json and instantiates IBackendProvider instances.
///
/// Supports:
///   - Declarative JSON config with env-var references (${VAR})
///   - Built-in provider factories (registered via REGISTER_BACKEND macro)
///   - DLL-based providers loaded through PluginLoader
///   - Hot-reload via QFileSystemWatcher
class BackendLoader : public QObject
{
    Q_OBJECT

public:
    explicit BackendLoader(QObject *parent = nullptr);
    ~BackendLoader() override;

    // Non-copyable, non-movable.
    BackendLoader(const BackendLoader &) = delete;
    BackendLoader &operator=(const BackendLoader &) = delete;
    BackendLoader(BackendLoader &&) = delete;
    BackendLoader &operator=(BackendLoader &&) = delete;

    // ---- PluginLoader integration ----

    /// Set an optional PluginLoader for DLL-based providers.
    /// When a provider type matches no built-in factory, BackendLoader
    /// delegates to PluginLoader (loads <type>_backend.dll and casts
    /// the resulting IPlugin to IBackendProvider).
    void setPluginLoader(PluginLoader *loader);

    // ---- Config I/O ----

    /// Load providers from a JSON config file ("providers.json").
    /// The root object must contain a "providers" array.
    /// Returns true on success; call lastError() on failure.
    bool loadFromJson(const std::string &path);

    /// Reload from the last successfully loaded file.
    bool reload();

    /// Human-readable error from the last failed operation.
    std::string lastError() const { return lastError_; }

    /// Path of the last loaded file (empty if never loaded).
    std::string filePath() const { return filePath_; }

    // ---- Hot-reload ----

    /// Enable/disable QFileSystemWatcher-based hot-reload.
    /// When the watched file changes on disk, providers are automatically
    /// reloaded and the signal providersChanged() is emitted.
    void enableHotReload(bool enable = true);

    /// Returns true if hot-reload is currently active.
    bool isHotReloadEnabled() const;

    // ---- Provider access ----

    /// Get a provider by its configured name.
    kathub::ai::IBackendProvider *provider(const std::string &name) const;

    /// All currently loaded providers.
    std::vector<kathub::ai::IBackendProvider *> allProviders() const;

    /// Number of loaded providers.
    size_t providerCount() const;

    // ---- Factory registry (for built-in providers) ----

    /// Register a factory for a provider type.
    /// Built-in backends call this at static-init time via REGISTER_BACKEND.
    static void registerFactory(const std::string &type,
                                BackendProviderFactory factory);

signals:
    /// Emitted after a successful load or hot-reload.
    void providersChanged();

    /// Emitted when a provider fails to load (hot-reload path).
    void providerLoadFailed(const QString &name, const QString &error);

private:
    /// Resolve ${VAR_NAME} references against the process environment.
    static std::string resolveEnvVar(const std::string &raw);

    /// Parse one entry from the "providers" array.
    std::optional<ProviderConfig>
    parseProviderEntry(const QJsonObject &obj, int index) const;

    /// Create (or reload) a single provider from its config.
    kathub::ai::IBackendProvider *instantiateProvider(const ProviderConfig &cfg);

    /// Unload and destroy all tracked providers.
    void unloadAll();

    /// Re-read the file and replace all providers (core of load + hot-reload).
    bool loadImpl(const std::string &path);

    std::string filePath_;
    mutable std::string lastError_;

    // name → owning unique_ptr
    std::unordered_map<std::string,
                       std::unique_ptr<kathub::ai::IBackendProvider>>
        providers_;

    PluginLoader *pluginLoader_ = nullptr;

    // QFileSystemWatcher for hot-reload.
    std::unique_ptr<QFileSystemWatcher> fileWatcher_;

    // Built-in factory registry.
    static std::unordered_map<std::string, BackendProviderFactory> &
    factories();
};

// ===========================================================================
//  REGISTER_BACKEND macro
// ===========================================================================
/// Place in a .cpp file to register a built-in backend provider:
///
///   #include <kathub/backend/BackendLoader.h>
///   REGISTER_BACKEND("openai", OpenAIBackend)
///
/// The class must derive from IBackendProvider and be default-constructible.
/// BackendLoader will call initialize() with a JSON string built from
/// the ProviderConfig fields.
#define REGISTER_BACKEND(Type, ClassName)                                      \
    namespace                                                                  \
    {                                                                          \
        static bool _kathub_backend_##ClassName = []() -> bool {               \
            kathub::backend::BackendLoader::registerFactory(                   \
                Type,                                                          \
                [](const kathub::backend::ProviderConfig &cfg)                 \
                    -> std::unique_ptr<kathub::ai::IBackendProvider> {         \
                    auto p = std::make_unique<ClassName>();                    \
                    QJsonObject configJson;                                    \
                    configJson[QStringLiteral("endpoint")] = QString::fromStdString(cfg.endpoint); \
                    configJson[QStringLiteral("api_key")]  = QString::fromStdString(cfg.apiKey); \
                    configJson[QStringLiteral("model")]    = QString::fromStdString(cfg.model); \
                    configJson[QStringLiteral("parameters")] = cfg.parameters; \
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

} // namespace backend
} // namespace kathub
