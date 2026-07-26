#include "AppPaths.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace KatHub {

QString AppPaths::s_dataDir;
QMap<QString, QString> AppPaths::s_customPaths;

void AppPaths::init(const QString &dataDir)
{
    s_dataDir = QDir::fromNativeSeparators(dataDir);
    loadCustomPaths();
}

// ── Base directories ──────────────────────────────────────────

QString AppPaths::dataDir()
{
    return resolve(KEY_DATA_DIR, s_dataDir);
}

QString AppPaths::configDir()
{
    return resolve(KEY_CONFIG_DIR,
                   dataDir() + QStringLiteral("/config"));
}

QString AppPaths::logsDir()
{
    return resolve(KEY_LOGS_DIR,
                   dataDir() + QStringLiteral("/logs"));
}

QString AppPaths::pluginsDir()
{
    return resolve(KEY_PLUGINS_DIR,
                   dataDir() + QStringLiteral("/plugins"));
}

// ── Custom paths ──────────────────────────────────────────────

QString AppPaths::customPath(const QString &key)
{
    return s_customPaths.value(key);
}

void AppPaths::setCustomPath(const QString &key, const QString &path)
{
    s_customPaths[key] = path;
    saveCustomPaths();
}

void AppPaths::loadCustomPaths()
{
    QSettings settings(QStringLiteral("KatHub"), QStringLiteral("Paths"));
    for (const QString &key : settings.allKeys()) {
        s_customPaths[key] = settings.value(key).toString();
    }
}

void AppPaths::saveCustomPaths()
{
    QSettings settings(QStringLiteral("KatHub"), QStringLiteral("Paths"));
    settings.clear();
    for (auto it = s_customPaths.begin(); it != s_customPaths.end(); ++it) {
        if (!it.value().isEmpty())
            settings.setValue(it.key(), it.value());
    }
}

QString AppPaths::resolve(const QString &customKey,
                          const QString &defaultPath)
{
    if (s_customPaths.contains(customKey)
        && !s_customPaths[customKey].isEmpty())
        return s_customPaths[customKey];
    return defaultPath;
}

} // namespace KatHub
