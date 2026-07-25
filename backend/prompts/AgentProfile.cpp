#include "AgentProfile.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>

#include <iostream>

namespace KatHub {

// ============================================================================
//  AgentProfile
// ============================================================================

bool AgentProfile::isSystemPromptPath() const
{
    return systemPrompt.startsWith(QStringLiteral("templates/"))
        || systemPrompt.endsWith(QStringLiteral(".md"))
        || systemPrompt.endsWith(QStringLiteral(".txt"));
}

// ============================================================================
//  JSON (de)serialization
// ============================================================================

AgentProfile AgentProfile::fromJson(const QJsonObject &obj)
{
    AgentProfile p;
    p.name         = obj.value(QStringLiteral("name")).toString();
    p.systemPrompt = obj.value(QStringLiteral("systemPrompt")).toString();
    p.temperature  = obj.value(QStringLiteral("temperature")).toDouble(0.7);
    p.maxTokens    = obj.value(QStringLiteral("maxTokens")).toInt(4096);
    p.model        = obj.value(QStringLiteral("model")).toString();
    p.description  = obj.value(QStringLiteral("description")).toString();
    return p;
}

QJsonObject AgentProfile::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("name")]         = name;
    obj[QStringLiteral("systemPrompt")] = systemPrompt;
    obj[QStringLiteral("temperature")]  = temperature;
    obj[QStringLiteral("maxTokens")]    = maxTokens;
    if (!model.isEmpty())
        obj[QStringLiteral("model")] = model;
    if (!description.isEmpty())
        obj[QStringLiteral("description")] = description;
    return obj;
}

QList<AgentProfile> AgentProfile::loadFromJson(const QByteArray &json)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(json, &err);

    if (err.error != QJsonParseError::NoError) {
        std::cerr << "[AgentProfile] JSON parse error: "
                  << err.errorString().toStdString() << std::endl;
        return {};
    }

    if (!doc.isObject()) {
        std::cerr << "[AgentProfile] Root must be a JSON object" << std::endl;
        return {};
    }

    QJsonObject root = doc.object();
    QJsonArray arr = root.value(QStringLiteral("profiles")).toArray();

    QList<AgentProfile> profiles;
    profiles.reserve(arr.size());

    for (const auto &val : arr) {
        if (val.isObject()) {
            profiles.append(fromJson(val.toObject()));
        }
    }

    return profiles;
}

QList<AgentProfile> AgentProfile::loadFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "[AgentProfile] Cannot open: "
                  << path.toStdString() << std::endl;
        return defaults();
    }

    QByteArray data = file.readAll();
    file.close();

    QList<AgentProfile> profiles = loadFromJson(data);
    if (profiles.isEmpty()) {
        std::cerr << "[AgentProfile] No profiles found in "
                  << path.toStdString() << ", using defaults." << std::endl;
        return defaults();
    }

    return profiles;
}

AgentProfile AgentProfile::find(const QList<AgentProfile> &profiles,
                                const QString &name)
{
    for (const auto &p : profiles) {
        if (p.name == name)
            return p;
    }

    // Fallback to defaults.
    for (const auto &p : defaults()) {
        if (p.name == name)
            return p;
    }

    std::cerr << "[AgentProfile] Profile not found: "
              << name.toStdString() << ", returning empty." << std::endl;
    return {};
}

QList<AgentProfile> AgentProfile::defaults()
{
    return {
        {
            QStringLiteral("default"),
            QStringLiteral("templates/default-system.md"),
            0.7,
            4096,
            {},
            QStringLiteral("General-purpose assistant")
        },
        {
            QStringLiteral("coder"),
            QStringLiteral("templates/coder-system.md"),
            0.3,
            8192,
            {},
            QStringLiteral("Code generation and review")
        }
    };
}

} // namespace KatHub
