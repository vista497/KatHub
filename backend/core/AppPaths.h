#pragma once

#include <QString>
#include <QMap>

namespace KatHub {

/// Centralized application paths.
///
/// All paths are computed relative to the application data directory.
/// Custom overrides can be stored via setCustomPath() / QSettings.
class AppPaths
{
public:
    AppPaths() = delete;

    /// Initialize with the application data directory.
    static void init(const QString &dataDir);

    /// Base data directory.
    static QString dataDir();

    /// Configuration directory (JSON configs, providers.json, etc.).
    static QString configDir();

    /// Logs directory.
    static QString logsDir();

    /// Plugins directory (DLL-based providers).
    static QString pluginsDir();

    // ── Custom path overrides (persisted in QSettings) ──────

    static QString customPath(const QString &key);
    static void    setCustomPath(const QString &key, const QString &path);
    static void    loadCustomPaths();
    static void    saveCustomPaths();

    static constexpr const char *KEY_DATA_DIR   = "paths/data";
    static constexpr const char *KEY_CONFIG_DIR  = "paths/config";
    static constexpr const char *KEY_LOGS_DIR    = "paths/logs";
    static constexpr const char *KEY_PLUGINS_DIR = "paths/plugins";

private:
    static QString resolve(const QString &customKey,
                           const QString &defaultPath);

    static QString                  s_dataDir;
    static QMap<QString, QString>   s_customPaths;
};

} // namespace KatHub
