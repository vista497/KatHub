#include "ConfigService.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcessEnvironment>

#include <set>
#include <sstream>

// ============================================================================
//  Construction
// ============================================================================

ConfigService::ConfigService()  = default;
ConfigService::~ConfigService() = default;

// ============================================================================
//  File I/O
// ============================================================================

bool ConfigService::loadFromFile(const std::string &path)
{
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::ostringstream oss;
        oss << "Cannot open config file: " << path
            << " (" << file.errorString().toStdString() << ")";
        lastError_ = oss.str();
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        std::ostringstream oss;
        oss << "JSON parse error in " << path
            << " at offset " << parseError.offset
            << ": " << parseError.errorString().toStdString();
        lastError_ = oss.str();
        return false;
    }

    if (!doc.isObject()) {
        lastError_ = "Config root must be a JSON object";
        return false;
    }

    config_   = doc.object();
    filePath_ = path;
    lastError_.clear();
    return true;
}

bool ConfigService::reload()
{
    if (filePath_.empty()) {
        lastError_ = "No file loaded yet — nothing to reload";
        return false;
    }
    return loadFromFile(filePath_);
}

std::string ConfigService::lastError() const
{
    return lastError_;
}

// ============================================================================
//  Path navigation
// ============================================================================

QJsonValue ConfigService::navigatePath(const std::string &path) const
{
    if (path.empty())
        return QJsonValue::Undefined;

    std::istringstream stream(path);
    std::string segment;
    QJsonValue current = QJsonValue(config_);

    while (std::getline(stream, segment, '.')) {
        if (!current.isObject())
            return QJsonValue::Undefined;

        QJsonObject obj = current.toObject();
        QString key = QString::fromStdString(segment);
        if (!obj.contains(key))
            return QJsonValue::Undefined;

        current = obj.value(key);
    }

    return current;
}

// ============================================================================
//  Value access
// ============================================================================

std::optional<QVariant> ConfigService::get(const std::string &path) const
{
    QJsonValue val = navigatePath(path);
    if (val.isUndefined())
        return std::nullopt;

    return applyEnvOverride(path, val);
}

QVariant ConfigService::get(const std::string &path,
                            const QVariant &defaultValue) const
{
    std::optional<QVariant> opt = get(path);
    return opt.has_value() ? *opt : defaultValue;
}

bool ConfigService::has(const std::string &path) const
{
    return !navigatePath(path).isUndefined();
}

// ============================================================================
//  Environment overrides
// ============================================================================

void ConfigService::setEnvOverride(const std::string &configPath,
                                   const std::string &envVar)
{
    envOverrides_[configPath] = envVar;
}

QVariant ConfigService::applyEnvOverride(const std::string &path,
                                         const QJsonValue &fileValue) const
{
    auto it = envOverrides_.find(path);
    if (it == envOverrides_.end())
        return fileValue.toVariant();

    const QString envName = QString::fromStdString(it->second);
    const QString envVal  = QProcessEnvironment::systemEnvironment().value(envName);
    if (envVal.isEmpty() && !QProcessEnvironment::systemEnvironment().contains(envName))
        return fileValue.toVariant();   // env var not set — use file value

    // Parse the env-var value according to the file value's JSON type.
    if (fileValue.isDouble()) {
        bool ok = false;
        int intVal = envVal.toInt(&ok);
        if (ok) return QVariant(intVal);
        double dblVal = envVal.toDouble(&ok);
        if (ok) return QVariant(dblVal);
        return QVariant(envVal);   // fallback: return as string
    }

    if (fileValue.isBool()) {
        const QString lower = envVal.toLower();
        if (lower == QStringLiteral("true") || lower == QStringLiteral("1")
            || lower == QStringLiteral("yes"))
            return QVariant(true);
        if (lower == QStringLiteral("false") || lower == QStringLiteral("0")
            || lower == QStringLiteral("no"))
            return QVariant(false);
        return QVariant(envVal);   // unrecognised — return as string
    }

    // Default: return as string.
    return QVariant(envVal);
}

// ============================================================================
//  Schema validation
// ============================================================================

bool ConfigService::checkType(const QJsonValue &value,
                              const QString &expectedType) const
{
    if (expectedType == QStringLiteral("string"))
        return value.isString();
    if (expectedType == QStringLiteral("integer")) {
        if (!value.isDouble())
            return false;
        double d = value.toDouble();
        return d == static_cast<double>(static_cast<qint64>(d));
    }
    if (expectedType == QStringLiteral("number"))
        return value.isDouble();
    if (expectedType == QStringLiteral("boolean"))
        return value.isBool();
    if (expectedType == QStringLiteral("object"))
        return value.isObject();
    if (expectedType == QStringLiteral("array"))
        return value.isArray();

    // Unknown type — accept (lenient).
    return true;
}

bool ConfigService::validateObject(const QJsonObject &obj,
                                   const QJsonObject &schema,
                                   const QString &prefix) const
{
    // Check "required" array.
    if (schema.contains(QStringLiteral("required"))) {
        QJsonValue reqVal = schema.value(QStringLiteral("required"));
        if (!reqVal.isArray()) {
            std::ostringstream oss;
            oss << prefix.toStdString()
                << ": \"required\" must be an array";
            lastError_ = oss.str();
            return false;
        }
        const QJsonArray required = reqVal.toArray();
        for (const QJsonValue &r : required) {
            if (!r.isString()) {
                std::ostringstream oss;
                oss << prefix.toStdString()
                    << ": \"required\" entries must be strings";
                lastError_ = oss.str();
                return false;
            }
            QString propName = r.toString();
            if (!obj.contains(propName)) {
                std::ostringstream oss;
                oss << prefix.toStdString()
                    << ": missing required property \""
                    << propName.toStdString() << "\"";
                lastError_ = oss.str();
                return false;
            }
        }
    }

    // Check "properties".
    if (schema.contains(QStringLiteral("properties"))) {
        QJsonValue propsVal = schema.value(QStringLiteral("properties"));
        if (!propsVal.isObject()) {
            std::ostringstream oss;
            oss << prefix.toStdString()
                << ": \"properties\" must be an object";
            lastError_ = oss.str();
            return false;
        }
        const QJsonObject props = propsVal.toObject();
        for (auto it = props.begin(); it != props.end(); ++it) {
            const QString   propName   = it.key();
            const QJsonObject propSchema = it.value().toObject();

            // If the config doesn't have this property, skip it
            // (it might be optional — only complain if it's in "required").
            if (!obj.contains(propName)) {
                std::set<QString> requiredSet;
                if (schema.contains(QStringLiteral("required"))) {
                    const QJsonArray reqArr = schema.value(QStringLiteral("required")).toArray();
                    for (const auto &r : reqArr)
                        requiredSet.insert(r.toString());
                }
                if (requiredSet.count(propName)) {
                    std::ostringstream oss;
                    oss << prefix.toStdString() << "."
                        << propName.toStdString()
                        << ": missing required property";
                    lastError_ = oss.str();
                    return false;
                }
                continue;   // optional property — skip validation
            }

            QJsonValue val = obj.value(propName);
            QString childPrefix = prefix.isEmpty()
                ? propName
                : prefix + QStringLiteral(".") + propName;

            // Type check.
            if (propSchema.contains(QStringLiteral("type"))) {
                QJsonValue typeVal = propSchema.value(QStringLiteral("type"));
                if (!typeVal.isString()) {
                    std::ostringstream oss;
                    oss << childPrefix.toStdString()
                        << ": \"type\" must be a string";
                    lastError_ = oss.str();
                    return false;
                }
                QString typeStr = typeVal.toString();
                if (!checkType(val, typeStr)) {
                    std::ostringstream oss;
                    oss << childPrefix.toStdString()
                        << ": expected type \"" << typeStr.toStdString()
                        << "\", got " << (val.isDouble() ? "number"
                                         : val.isString() ? "string"
                                         : val.isBool()   ? "boolean"
                                         : val.isObject() ? "object"
                                         : val.isArray()  ? "array"
                                         : val.isNull()   ? "null"
                                         :                  "unknown");
                    lastError_ = oss.str();
                    return false;
                }
            }

            // Recurse into nested objects.
            if (propSchema.contains(QStringLiteral("properties"))
                && val.isObject()) {
                if (!validateObject(val.toObject(), propSchema, childPrefix))
                    return false;
            }
        }
    }

    return true;
}

bool ConfigService::validate(const std::string &schemaJson)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromStdString(schemaJson), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        std::ostringstream oss;
        oss << "Schema JSON parse error at offset " << parseError.offset
            << ": " << parseError.errorString().toStdString();
        lastError_ = oss.str();
        return false;
    }

    if (!doc.isObject()) {
        lastError_ = "Schema must be a JSON object";
        return false;
    }

    QJsonObject schema = doc.object();

    // Root type must be "object".
    if (schema.contains(QStringLiteral("type"))) {
        QString rootType = schema.value(QStringLiteral("type")).toString();
        if (rootType != QStringLiteral("object")) {
            lastError_ = "Root schema type must be \"object\"";
            return false;
        }
    }

    return validateObject(config_, schema, QString());
}
