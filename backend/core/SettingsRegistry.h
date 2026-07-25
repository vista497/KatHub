#pragma once

#include <QObject>
#include <QSettings>
#include <QVariant>
#include <QString>

namespace KatHub {

/// Thin wrapper around QSettings with a settings-changed signal.
///
/// Usage:
///   SettingsRegistry reg("KatHub", "General");
///   reg.setValue("theme", "dark");
///   auto theme = reg.value("theme", "light").toString();
///
/// The settings-changed signal is emitted automatically after setValue().
class SettingsRegistry : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SettingsRegistry)

public:
    /// Construct with organization and application names
    /// (forwarded to QSettings).
    explicit SettingsRegistry(const QString &organization,
                              const QString &application,
                              QObject *parent = nullptr);

    ~SettingsRegistry() override;

    /// Get a settings value with an optional default.
    QVariant value(const QString &key,
                   const QVariant &defaultValue = QVariant()) const;

    /// Set a settings value. Emits settingChanged().
    void setValue(const QString &key, const QVariant &value);

    /// Check if a key exists.
    bool contains(const QString &key) const;

    /// Remove a key. Emits settingChanged().
    void remove(const QString &key);

    /// Get all keys.
    QStringList allKeys() const;

    /// Begin / end group (same as QSettings).
    void beginGroup(const QString &prefix);
    void endGroup();

    /// Access underlying QSettings (use sparingly).
    QSettings &settings() { return m_settings; }
    const QSettings &settings() const { return m_settings; }

signals:
    /// Emitted when any setting is changed via setValue() or remove().
    void settingChanged(const QString &key, const QVariant &newValue);

private:
    QSettings m_settings;
};

} // namespace KatHub
