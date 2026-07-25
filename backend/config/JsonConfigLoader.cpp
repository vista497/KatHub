#include "JsonConfigLoader.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcessEnvironment>

#include <iostream>

// ============================================================================
//  load()
// ============================================================================

bool JsonConfigLoader::load(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cerr << "JsonConfigLoader: cannot open " << filePath.toStdString() << std::endl;
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        std::cerr << "JsonConfigLoader: parse error at offset "
                  << parseError.offset << ": "
                  << parseError.errorString().toStdString() << std::endl;
        return false;
    }

    if (!doc.isObject()) {
        std::cerr << "JsonConfigLoader: root must be a JSON object" << std::endl;
        return false;
    }

    values_.clear();
    flatten(doc.object(), QString());

    loaded_ = true;
    return true;
}

// ============================================================================
//  applyEnvOverrides()
// ============================================================================

void JsonConfigLoader::applyEnvOverrides()
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QStringList keys = env.keys();

    for (const QString &key : keys) {
        if (!key.startsWith(QStringLiteral("KATHUB_")))
            continue;

        const QString configKey = envToKey(key);
        const QString envValue = env.value(key);

        // Parse the value — try integer, then boolean, else string.
        bool ok = false;
        const int intVal = envValue.toInt(&ok);
        if (ok) {
            values_[configKey] = intVal;
            continue;
        }

        if (envValue.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
            values_[configKey] = true;
            continue;
        }
        if (envValue.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0) {
            values_[configKey] = false;
            continue;
        }

        values_[configKey] = envValue;
    }
}

// ============================================================================
//  value()
// ============================================================================

QJsonValue JsonConfigLoader::value(const QString &key) const
{
    auto it = values_.constFind(key);
    if (it != values_.constEnd())
        return *it;
    return QJsonValue::Undefined;
}

// ============================================================================
//  stringValue()
// ============================================================================

QString JsonConfigLoader::stringValue(const QString &key, const QString &defaultValue) const
{
    const QJsonValue v = value(key);
    if (v.isUndefined())
        return defaultValue;
    if (v.isString())
        return v.toString();
    // Coerce numbers / bools to string
    if (v.isDouble())
        return QString::number(v.toDouble());
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    return defaultValue;
}

// ============================================================================
//  intValue()
// ============================================================================

int JsonConfigLoader::intValue(const QString &key, int defaultValue) const
{
    const QJsonValue v = value(key);
    if (v.isUndefined())
        return defaultValue;
    if (v.isDouble())
        return static_cast<int>(v.toDouble());
    if (v.isString()) {
        bool ok = false;
        const int result = v.toString().toInt(&ok);
        return ok ? result : defaultValue;
    }
    return defaultValue;
}

// ============================================================================
//  boolValue()
// ============================================================================

bool JsonConfigLoader::boolValue(const QString &key, bool defaultValue) const
{
    const QJsonValue v = value(key);
    if (v.isUndefined())
        return defaultValue;
    if (v.isBool())
        return v.toBool();
    if (v.isString()) {
        const QString s = v.toString().trimmed().toLower();
        if (s == QStringLiteral("true") || s == QStringLiteral("1"))
            return true;
        if (s == QStringLiteral("false") || s == QStringLiteral("0"))
            return false;
    }
    if (v.isDouble())
        return v.toDouble() != 0.0;
    return defaultValue;
}

// ============================================================================
//  contains()
// ============================================================================

bool JsonConfigLoader::contains(const QString &key) const
{
    return values_.contains(key);
}

// ============================================================================
//  flatten()
// ============================================================================

void JsonConfigLoader::flatten(const QJsonObject &obj, const QString &prefix)
{
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const QString fullKey = prefix.isEmpty()
            ? it.key()
            : prefix + QStringLiteral(".") + it.key();

        const QJsonValue &val = it.value();

        if (val.isObject()) {
            flatten(val.toObject(), fullKey);
        } else {
            values_[fullKey] = val;
        }
    }
}

// ============================================================================
//  envToKey()
// ============================================================================

QString JsonConfigLoader::envToKey(const QString &envName)
{
    // Strip KATHUB_ prefix (case-sensitive).
    QString key = envName.mid(7); // length of "KATHUB_"

    // Convert to lower case and replace underscores with dots.
    key = key.toLower();
    key.replace(QLatin1Char('_'), QLatin1Char('.'));

    return key;
}
