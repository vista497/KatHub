#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVariant>

#include <optional>
#include <string>
#include <unordered_map>

/// Thread-safe configuration service.
///
/// Loads JSON config from file, validates against a JSON Schema
/// (supported subset: "type", "properties", "required", nested objects),
/// resolves environment-variable overrides, and provides dot-path access
/// with optional defaults.
///
/// Plugins access this through the opaque HostApi::configService pointer.
class ConfigService
{
public:
    ConfigService();
    ~ConfigService();

    // Non-copyable, non-movable (host owns the single instance).
    ConfigService(const ConfigService &) = delete;
    ConfigService &operator=(const ConfigService &) = delete;
    ConfigService(ConfigService &&) = delete;
    ConfigService &operator=(ConfigService &&) = delete;

    // ---------------------------------------------------------------
    //  File I/O
    // ---------------------------------------------------------------

    /// Load configuration from a JSON file.  Returns true on success.
    /// On failure, call lastError() for details.
    bool loadFromFile(const std::string &path);

    /// Reload the configuration from the last successfully loaded file.
    /// Returns true on success.
    bool reload();

    /// Return the last error message (empty string if no error).
    std::string lastError() const;

    /// Return the last loaded file path (empty if never loaded).
    std::string filePath() const { return filePath_; }

    // ---------------------------------------------------------------
    //  Validation
    // ---------------------------------------------------------------

    /// Validate the loaded config against a JSON Schema string.
    ///
    /// The schema must be a JSON object with the following supported
    /// keywords (JSON Schema subset):
    ///   - "type": "object" | "string" | "integer" | "number" | "boolean" | "array"
    ///   - "properties": { ... }  (nested object properties)
    ///   - "required": [ ... ]    (array of property names)
    ///
    /// Returns true if the config passes validation.
    /// On failure, call lastError() for a human-readable message.
    bool validate(const std::string &schemaJson);

    // ---------------------------------------------------------------
    //  Value access
    // ---------------------------------------------------------------

    /// Get a value by dot-separated path (e.g. "server.port").
    /// Returns std::nullopt if the path does not exist.
    std::optional<QVariant> get(const std::string &path) const;

    /// Get a value with a fallback default.
    QVariant get(const std::string &path, const QVariant &defaultValue) const;

    /// Check whether a path exists in the config.
    bool has(const std::string &path) const;

    // ---------------------------------------------------------------
    //  Environment variable overrides
    // ---------------------------------------------------------------

    /// Register an environment variable that overrides a config path.
    /// When get() is called for \p configPath and the named \p envVar
    /// is set in the environment, its value takes precedence over the
    /// file value.  The value is parsed according to the JSON type:
    /// integers if the file value is a number, otherwise a string.
    void setEnvOverride(const std::string &configPath,
                        const std::string &envVar);

    // ---------------------------------------------------------------
    //  Test helpers
    // ---------------------------------------------------------------

    /// Return the raw config object (for test inspection).
    const QJsonObject &rawConfig() const { return config_; }

private:
    QJsonObject config_;
    std::string filePath_;
    mutable std::string lastError_;
    std::unordered_map<std::string, std::string> envOverrides_;

    // Navigate a dot-separated path into the config tree.
    // Returns QJsonValue::Undefined if the path does not exist.
    QJsonValue navigatePath(const std::string &path) const;

    // Apply any registered environment-variable override.
    QVariant applyEnvOverride(const std::string &path,
                              const QJsonValue &fileValue) const;

    // Validate a single value against an expected type string.
    bool checkType(const QJsonValue &value, const QString &expectedType) const;

    // Recursively validate an object against a schema object.
    bool validateObject(const QJsonObject &obj, const QJsonObject &schema,
                        const QString &prefix) const;
};
