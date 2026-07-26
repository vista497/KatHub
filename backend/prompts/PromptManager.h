#pragma once

#include <QHash>
#include <QString>
#include <QVariantMap>

#include <mutex>

namespace KatHub {

/// Manages prompt templates loaded from .md/.txt files.
///
/// Supports:
///   - Loading templates from a base directory
///   - {{variable}} substitution from QVariantMap
///   - In-memory caching (does not re-read files on every call)
///
/// Thread-safe for concurrent reads after initial loading.
///
/// Usage:
///   PromptManager pm;
///   pm.setBaseDir("/path/to/prompts");
///   QString result = pm.process("greeting", {{"name", "Alice"}});
class PromptManager
{
public:
    PromptManager();
    explicit PromptManager(const QString &baseDir);
    ~PromptManager();

    // ---- Configuration ----

    /// Set the base directory for template files.
    void setBaseDir(const QString &dir);

    /// Returns the current base directory.
    QString baseDir() const;

    // ---- Loading ----

    /// Load a template from disk (reads file, caches result).
    /// @param name  Template name without extension (e.g. "default-system").
    /// @return Raw template content, or empty string on failure.
    QString loadTemplate(const QString &name);

    /// Load a template from an explicit file path.
    /// @param path  Absolute or relative path to the template file.
    /// @param cacheKey  Optional key for caching. If empty, path is used.
    QString loadFile(const QString &path, const QString &cacheKey = {});

    /// Reload a template from disk (bypass cache).
    QString reload(const QString &name);

    // ---- Processing ----

    /// Load template by name and substitute {{variables}}.
    /// @param name   Template name (without extension).
    /// @param vars   Variable map for substitution.
    /// @return Processed string with all {{key}} replaced by vars["key"].
    QString process(const QString &name, const QVariantMap &vars = {});

    /// Substitute {{variables}} in a raw template string.
    /// Supports {{key}} and optional default {{key:default}}.
    static QString substitute(const QString &tmpl, const QVariantMap &vars);

    // ---- Cache management ----

    /// Remove all cached templates.
    void clearCache();

    /// Remove a specific template from cache.
    void invalidate(const QString &name);

    /// Number of cached templates.
    int cacheSize() const;

private:
    /// Read entire file contents. Returns empty string on failure.
    static QString readFile(const QString &path);

    /// Resolve a template name to a file path (tries .md, then .txt).
    QString resolvePath(const QString &name) const;

    QString m_baseDir;
    QHash<QString, QString> m_cache;
    mutable std::mutex m_mutex;
};

} // namespace KatHub
