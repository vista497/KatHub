#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>

namespace KatHub {

/// Describes an AI agent profile with its system prompt and generation parameters.
///
/// Profiles are loaded from a profiles.json configuration file.
/// The systemPrompt field can be either a file path (relative to the prompts
/// directory) or inline text.
struct AgentProfile
{
    QString name;          // Unique profile name (e.g. "default", "coder")
    QString systemPrompt;  // Either a file path (relative) or inline prompt text
    double  temperature = 0.7;
    int     maxTokens   = 4096;
    QString model;         // Optional: specific model override
    QString description;   // Human-readable description

    /// Returns true if systemPrompt looks like a file path (starts with
    /// "templates/" or contains ".md"/".txt").
    bool isSystemPromptPath() const;

    /// Load all profiles from a JSON file.
    /// Expected format:
    ///   { "profiles": [ { "name":"...", "systemPrompt":"...", ... }, ... ] }
    static QList<AgentProfile> loadFromFile(const QString &path);

    /// Load all profiles from JSON data (QByteArray).
    static QList<AgentProfile> loadFromJson(const QByteArray &json);

    /// Parse a single profile from a QJsonObject.
    static AgentProfile fromJson(const QJsonObject &obj);

    /// Convert this profile to a QJsonObject.
    QJsonObject toJson() const;

    /// Find a profile by name. Returns a default-constructed profile if not found.
    static AgentProfile find(const QList<AgentProfile> &profiles,
                             const QString &name);

    /// Default profiles (built-in fallback if profiles.json is missing).
    static QList<AgentProfile> defaults();
};

} // namespace KatHub
