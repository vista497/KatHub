#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace KatHub {

/// Loads and queries configuration from a JSON file.
///
/// Handles three error conditions gracefully:
///   - File not found → use defaults (empty JSON object)
///   - Malformed JSON   → log error via std::cerr, use defaults
///   - Missing key path  → return the supplied default value
///
/// Supports nested key paths separated by '.' (e.g. "server.port").
///
/// Usage:
///   JsonConfigLoader cfg("kathub.json");
///   cfg.load();
///   int port = cfg.getInt("server.port", 8080);
class JsonConfigLoader
{
public:
    /// Construct with optional path to the JSON config file.
    explicit JsonConfigLoader(const std::string &path = "kathub.json");

    /// Load and parse the config file.
    /// Returns true on success.
    /// On failure, logs the error and resets internal data to an empty object.
    bool load();

    /// Apply KATHUB_* environment variable overrides.
    /// Env var values take precedence over JSON file values.
    /// Mapping: strip KATHUB_ prefix, lowercase, replace '_' with '.'.
    ///   KATHUB_SERVER_PORT  → server.port
    ///   KATHUB_LOG_LEVEL    → log.level
    /// Returns the number of overrides applied.
    int applyEnvOverrides();

    // ---- Typed accessors ----

    /// Get a string value at the dot-separated key path.
    std::string getString(const std::string &keyPath,
                          const std::string &defaultValue = "") const;

    /// Get an integer value at the dot-separated key path.
    int getInt(const std::string &keyPath, int defaultValue = 0) const;

    /// Get a boolean value at the dot-separated key path.
    bool getBool(const std::string &keyPath, bool defaultValue = false) const;

    /// Get a raw nlohmann::json value (or the provided default).
    nlohmann::json getValue(const std::string &keyPath,
                            const nlohmann::json &defaultValue = nullptr) const;

    // ---- State accessors ----

    /// Direct read-only access to the parsed JSON root.
    const nlohmann::json &json() const { return data_; }

    /// True if the last load() succeeded.
    bool loaded() const { return loaded_; }

    /// Human-readable description of the last error.
    const std::string &lastError() const { return lastError_; }

private:
    /// Walk a dot-separated path into the JSON tree.
    /// Returns nullptr (nlohmann::json()) if any segment is missing.
    nlohmann::json resolvePath(const std::string &keyPath) const;

    /// Set a value at a dot-separated path, creating intermediate objects.
    void setPath(const std::string &keyPath, const nlohmann::json &value);

    /// Convert KATHUB_* env var name to dot-notation config key.
    static std::string envToKey(const std::string &envName);

    /// Parse an env var string value to the appropriate JSON type.
    static nlohmann::json parseEnvValue(const std::string &rawValue);

    std::string     path_;
    nlohmann::json  data_;
    bool            loaded_    = false;
    std::string     lastError_;
};

} // namespace KatHub
