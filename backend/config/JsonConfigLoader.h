#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QString>

// ---------------------------------------------------------------------------
// JsonConfigLoader
//
// Loads a JSON configuration file and applies KATHUB_* environment variable
// overrides. Keys are stored in dot-notation (e.g. "server.port").
//
// Environment variables take precedence over JSON file values.
// Mapping rule: strip KATHUB_ prefix, convert to lowercase, replace
// underscores with dots.
//
//   KATHUB_SERVER_PORT  → server.port
//   KATHUB_LOG_LEVEL    → log.level
//   KATHUB_WS_PORT      → ws.port
// ---------------------------------------------------------------------------
class JsonConfigLoader
{
public:
    JsonConfigLoader() = default;
    ~JsonConfigLoader() = default;

    // Load a JSON configuration file. Returns true on success.
    bool load(const QString &filePath);

    // Apply KATHUB_* environment variable overrides.
    // Env var values replace JSON values for matching keys.
    void applyEnvOverrides();

    // Get a raw QJsonValue by dot-notation key. Returns QJsonValue::Undefined
    // if the key is not found.
    QJsonValue value(const QString &key) const;

    // Convenience typed accessors with defaults.
    QString stringValue(const QString &key, const QString &defaultValue = {}) const;
    int     intValue(const QString &key, int defaultValue = 0) const;
    bool    boolValue(const QString &key, bool defaultValue = false) const;

    // Check whether a key exists.
    bool contains(const QString &key) const;

    // Raw access to the flat map (for testing / debugging).
    const QMap<QString, QJsonValue> &values() const { return values_; }

    // Returns true if load() succeeded.
    bool isLoaded() const { return loaded_; }

private:
    // Flatten a JSON object into dot-notation keys, recursively.
    // prefix is the accumulated key prefix (e.g. "server").
    void flatten(const QJsonObject &obj, const QString &prefix);

    // Convert a KATHUB_* env var name to a dot-notation key.
    // "KATHUB_SERVER_PORT" → "server.port"
    static QString envToKey(const QString &envName);

    QMap<QString, QJsonValue> values_;
    bool loaded_ = false;
};
