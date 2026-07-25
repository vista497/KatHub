#include "SettingsRegistry.h"

namespace KatHub {

SettingsRegistry::SettingsRegistry(const QString &organization,
                                   const QString &application,
                                   QObject *parent)
    : QObject(parent)
    , m_settings(organization, application)
{
}

SettingsRegistry::~SettingsRegistry() = default;

QVariant SettingsRegistry::value(const QString &key,
                                 const QVariant &defaultValue) const
{
    return m_settings.value(key, defaultValue);
}

void SettingsRegistry::setValue(const QString &key, const QVariant &value)
{
    m_settings.setValue(key, value);
    m_settings.sync();
    emit settingChanged(key, value);
}

bool SettingsRegistry::contains(const QString &key) const
{
    return m_settings.contains(key);
}

void SettingsRegistry::remove(const QString &key)
{
    m_settings.remove(key);
    m_settings.sync();
    emit settingChanged(key, QVariant());
}

QStringList SettingsRegistry::allKeys() const
{
    return m_settings.allKeys();
}

void SettingsRegistry::beginGroup(const QString &prefix)
{
    m_settings.beginGroup(prefix);
}

void SettingsRegistry::endGroup()
{
    m_settings.endGroup();
}

} // namespace KatHub
