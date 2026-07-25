#include "kathub/config/ConfigService.h"

#include "kathtech/config/JsonConfigLoader.h"
#include "kathub/config/ConfigSchema.h"

#include <QFileInfo>

namespace KatHub {

// ============================================================================
//  Construction / Destruction
// ============================================================================

ConfigService::ConfigService(QString configPath,
                             kathub::config::ConfigSchema schema,
                             QObject *parent)
    : QObject(parent)
    , m_configPath(std::move(configPath))
    , m_loader(std::make_unique<JsonConfigLoader>(m_configPath.toStdString()))
    , m_schema(std::make_unique<kathub::config::ConfigSchema>(std::move(schema)))
{
}

ConfigService::~ConfigService() = default;

// ============================================================================
//  load  —  parse → validate → env-override → cache
// ============================================================================

bool ConfigService::load()
{
    m_lastError.clear();

    if (m_configPath.isEmpty())
    {
        m_lastError = QStringLiteral("Config path is empty");
        return false;
    }

    // 1.  Parse JSON from file via JsonConfigLoader
    if (!m_loader->load())
    {
        QFileInfo fi(m_configPath);
        if (!fi.exists())
        {
            // File missing → treat as empty config; schema defaults will fill
            m_loader = std::make_unique<JsonConfigLoader>(m_configPath.toStdString());
            // load() failed but we continue with empty object
        }
        else
        {
            m_lastError = QString::fromStdString(m_loader->lastError());
            return false;
        }
    }

    nlohmann::json parsed = m_loader->json();

    // 2.  Validate against schema (populates defaults for absent fields)
    auto result = m_schema->process(parsed);
    if (!result.valid)
    {
        QStringList errs;
        for (const auto &e : result.errors)
            errs.append(QString::fromStdString(e));
        m_lastError = QStringLiteral("Schema validation failed:\n")
                      + errs.join(QLatin1Char('\n'));
        return false;
    }

    // Use the validated data (with defaults applied)
    parsed = std::move(result.data);

    // 3.  Apply environment-variable overrides (KATHUB_*)
    m_loader->applyEnvOverrides();

    // 4.  Cache the validated + overridden config
    m_config = m_loader->json();
    m_loaded = true;

    return true;
}

// ============================================================================
//  reload  —  re-parses the same file, emits configChanged on delta
// ============================================================================

bool ConfigService::reload()
{
    const nlohmann::json prev = m_config;

    if (!load())
        return false;

    if (prev != m_config)
        emit configChanged();

    return true;
}

// ============================================================================
//  Accessors
// ============================================================================

const nlohmann::json &ConfigService::config() const
{
    return m_config;
}

QString ConfigService::lastError() const
{
    return m_lastError;
}

bool ConfigService::isLoaded() const
{
    return m_loaded;
}

nlohmann::json ConfigService::value(const std::string &keyPath,
                                     const nlohmann::json &defaultValue) const
{
    if (!m_loaded)
        return defaultValue;

    return m_loader->getValue(keyPath, defaultValue);
}

std::string ConfigService::string(const std::string &keyPath,
                                   const std::string &defaultValue) const
{
    return m_loader->getString(keyPath, defaultValue);
}

int ConfigService::integer(const std::string &keyPath, int defaultValue) const
{
    return m_loader->getInt(keyPath, defaultValue);
}

bool ConfigService::boolean(const std::string &keyPath, bool defaultValue) const
{
    return m_loader->getBool(keyPath, defaultValue);
}

} // namespace KatHub
