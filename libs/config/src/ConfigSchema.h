#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

namespace KatHub {

/// Describes one config field for schema validation.
struct ConfigField
{
    QString      keyPath;       ///< Dot-separated path, e.g. "server.port"
    QJsonValue::Type type = QJsonValue::Undefined;
    QJsonValue   defaultValue;
    bool         required = false;
};

/// Validates a QJsonObject against a declarative schema.
///
/// Usage:
///   ConfigSchema schema;
///   schema.addField({"server.port", QJsonValue::Double, 8080, true});
///   QStringList errors;
///   if (!schema.validate(config, &errors)) { ... }
class ConfigSchema
{
public:
    ConfigSchema() = default;

    /// Register a field definition.
    void addField(const ConfigField &field);
    void addField(ConfigField &&field);

    /// Validate @p config against registered fields.
    /// Sets missing optional fields to their default values.
    /// @param config   [in,out] Validated + corrected config object.
    /// @param errors   [out]    Human-readable error messages.
    /// @return true if validation passed (no errors appended to @p errors).
    bool validate(QJsonObject &config, QStringList *errors = nullptr) const;

    /// Convenience overload — does not modify config.
    bool isValid(const QJsonObject &config) const;

private:
    QJsonValue resolvePath(const QJsonObject &root, const QString &path) const;
    void       setPath(QJsonObject &root, const QString &path, const QJsonValue &value) const;

    QVector<ConfigField> m_fields;
};

} // namespace KatHub
