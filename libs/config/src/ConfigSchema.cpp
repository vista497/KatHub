#include "ConfigSchema.h"

namespace KatHub {

// ---------------------------------------------------------------------------
// Helpers — navigate a dot-separated path inside a QJsonObject
// ---------------------------------------------------------------------------

static QStringList splitPath(const QString &path)
{
    return path.split(QLatin1Char('.'), Qt::SkipEmptyParts);
}

QJsonValue ConfigSchema::resolvePath(const QJsonObject &root, const QString &path) const
{
    const QStringList parts = splitPath(path);
    if (parts.isEmpty())
        return QJsonValue(root);

    const QJsonObject *current = &root;
    for (int i = 0; i < parts.size(); ++i) {
        QJsonValue val = current->value(parts[i]);
        if (val.isUndefined() || val.isNull())
            return QJsonValue::Undefined;

        if (i == parts.size() - 1)
            return val;

        if (!val.isObject())
            return QJsonValue::Undefined;

        // Navigate deeper — store nested copy and point to it.
        static thread_local QJsonObject nestedHolder;
        nestedHolder = val.toObject();
        current = &nestedHolder;
    }
    return QJsonValue::Undefined;
}

void ConfigSchema::setPath(QJsonObject &root, const QString &path, const QJsonValue &value) const
{
    const QStringList parts = splitPath(path);
    if (parts.isEmpty())
        return;

    // Recursively set value at a dot-separated path, creating intermediate
    // objects as needed.
    auto setNested = [](QJsonObject &obj, const QStringList &p, int idx,
                         const QJsonValue &val, auto &self) -> void {
        if (idx == static_cast<int>(p.size()) - 1) {
            obj[p[idx]] = val;
            return;
        }
        QJsonObject nested = obj.value(p[idx]).toObject();
        self(nested, p, idx + 1, val, self);
        obj[p[idx]] = nested;
    };
    setNested(root, parts, 0, value, setNested);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ConfigSchema::addField(const ConfigField &field)
{
    m_fields.append(field);
}

void ConfigSchema::addField(ConfigField &&field)
{
    m_fields.append(std::move(field));
}

bool ConfigSchema::validate(QJsonObject &config, QStringList *errors) const
{
    bool ok = true;

    for (const auto &field : m_fields) {
        QJsonValue actual = resolvePath(config, field.keyPath);

        // --- required check ---
        if (field.required && (actual.isUndefined() || actual.isNull())) {
            if (errors)
                errors->append(QStringLiteral("Missing required field: '%1'").arg(field.keyPath));
            ok = false;
            continue;
        }

        // --- apply default if missing ---
        if ((actual.isUndefined() || actual.isNull()) && !field.defaultValue.isUndefined()) {
            setPath(config, field.keyPath, field.defaultValue);
            continue;
        }

        // --- type check (skip if type is Undefined = "any") ---
        if (field.type != QJsonValue::Undefined && actual.type() != field.type) {
            if (errors)
                errors->append(QStringLiteral("Field '%1': expected type %2, got %3")
                                   .arg(field.keyPath)
                                   .arg(static_cast<int>(field.type))
                                   .arg(static_cast<int>(actual.type())));
            ok = false;
        }
    }

    return ok;
}

bool ConfigSchema::isValid(const QJsonObject &config) const
{
    QJsonObject copy = config;
    return validate(copy);
}

} // namespace KatHub
