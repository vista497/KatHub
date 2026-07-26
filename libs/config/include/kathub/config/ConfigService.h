#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <string>

#include <nlohmann/json.hpp>

namespace KatHub {
class JsonConfigLoader;
}

namespace kathub::config {
class ConfigSchema;
}

namespace KatHub {

/// High-level config loading service.
///
/// Composes JsonConfigLoader (file I/O + env overrides) with
/// kathub::config::ConfigSchema (validation + defaults), adds caching
/// and a config-changed signal.
///
/// Usage:
///   auto schema = kathub::config::ConfigSchema::fromJson(schemaDef);
///   ConfigService svc("kathub.json", std::move(schema));
///   if (svc.load()) { ... }
///   connect(&svc, &ConfigService::configChanged, []{ ... });
class ConfigService : public QObject
{
    Q_OBJECT

public:
    /// @param configPath  Path to the JSON config file.
    /// @param schema      Schema definition used for validation.
    /// @param parent       Qt parent object.
    ConfigService(QString configPath,
                  kathub::config::ConfigSchema schema,
                  QObject *parent = nullptr);
    ~ConfigService() override;

    // ------------------------------------------------------------------------
    //  Lifecycle
    // ------------------------------------------------------------------------

    /// 1. Parse JSON via JsonConfigLoader
    /// 2. Validate against the schema (apply defaults)
    /// 3. Apply KATHUB_* environment-variable overrides
    /// 4. Cache the validated config
    bool load();

    /// Reload from the same file path.
    /// Emits configChanged() when the cached config actually changes.
    bool reload();

    // ------------------------------------------------------------------------
    //  Accessors
    // ------------------------------------------------------------------------

    /// The current validated + cached config (empty object if not loaded).
    const nlohmann::json &config() const;

    /// Human-readable last-error message (empty on success).
    QString lastError() const;

    /// True if at least one successful load() has completed.
    bool isLoaded() const;

    /// Access a nested value by dot-separated key path.
    nlohmann::json value(const std::string &keyPath,
                         const nlohmann::json &defaultValue = nullptr) const;

    /// Typed convenience.
    std::string string(const std::string &keyPath,
                       const std::string &defaultValue = {}) const;
    int integer(const std::string &keyPath, int defaultValue = 0) const;
    bool boolean(const std::string &keyPath, bool defaultValue = false) const;

signals:
    /// Emitted after a successful reload when the cached config differs.
    void configChanged();

private:
    QString m_configPath;
    nlohmann::json m_config;
    QString m_lastError;
    bool m_loaded = false;

    std::unique_ptr<JsonConfigLoader> m_loader;
    std::unique_ptr<kathub::config::ConfigSchema> m_schema;

    // Snapshot for reload change detection.
    nlohmann::json m_prevConfig;

    /// Set a value at a dot-separated path, creating intermediate objects.
    static void setNestedValue(nlohmann::json &root,
                               const QStringList &path,
                               const nlohmann::json &value);
};

} // namespace KatHub
