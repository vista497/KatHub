#include "PromptManager.h"

#include <QFile>
#include <QFileInfo>
#include <QPair>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTextStream>

#include <iostream>
#include <mutex>

namespace KatHub {

// ============================================================================
//  Construction
// ============================================================================

PromptManager::PromptManager()
    : m_baseDir(QStringLiteral("."))
{
}

PromptManager::PromptManager(const QString &baseDir)
    : m_baseDir(baseDir)
{
}

PromptManager::~PromptManager() = default;

// ============================================================================
//  Configuration
// ============================================================================

void PromptManager::setBaseDir(const QString &dir)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_baseDir = dir;
}

QString PromptManager::baseDir() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_baseDir;
}

// ============================================================================
//  Loading
// ============================================================================

QString PromptManager::loadTemplate(const QString &name)
{
    // Check cache first.
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_cache.constFind(name);
        if (it != m_cache.constEnd()) {
            return *it;
        }
    }

    QString path = resolvePath(name);
    if (path.isEmpty()) {
        std::cerr << "[PromptManager] Template not found: "
                  << name.toStdString() << std::endl;
        return {};
    }

    QString content = readFile(path);
    if (content.isEmpty()) {
        std::cerr << "[PromptManager] Failed to read: "
                  << path.toStdString() << std::endl;
        return {};
    }

    // Store in cache.
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache.insert(name, content);
    }

    return content;
}

QString PromptManager::loadFile(const QString &path, const QString &cacheKey)
{
    QString key = cacheKey.isEmpty() ? path : cacheKey;

    // Check cache.
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_cache.constFind(key);
        if (it != m_cache.constEnd()) {
            return *it;
        }
    }

    QString content = readFile(path);
    if (content.isEmpty()) {
        std::cerr << "[PromptManager] Failed to read file: "
                  << path.toStdString() << std::endl;
        return {};
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache.insert(key, content);
    }

    return content;
}

QString PromptManager::reload(const QString &name)
{
    // Remove from cache.
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache.remove(name);
    }

    return loadTemplate(name);
}

// ============================================================================
//  Processing
// ============================================================================

QString PromptManager::process(const QString &name, const QVariantMap &vars)
{
    QString tmpl = loadTemplate(name);
    if (tmpl.isEmpty()) {
        return {};
    }
    return substitute(tmpl, vars);
}

QString PromptManager::substitute(const QString &tmpl, const QVariantMap &vars)
{
    // Match {{key}} or {{key:default}}
    static const QRegularExpression re(
        QStringLiteral(R"(\{\{(\w+)(?::([^}]*))?\}\})"));

    QString result = tmpl;
    QRegularExpressionMatchIterator it = re.globalMatch(result);
    QList<QPair<int, QRegularExpressionMatch>> replacements;

    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        replacements.prepend(QPair<int, QRegularExpressionMatch>(
            m.capturedStart(), m));
    }

    for (const auto &pair : replacements) {
        const QRegularExpressionMatch &m = pair.second;
        QString key = m.captured(1);
        QString defaultValue = m.captured(2);

        QString replacement;
        if (vars.contains(key)) {
            replacement = vars.value(key).toString();
        } else if (!defaultValue.isNull()) {
            // Default was explicitly provided as {{key:default}}
            replacement = defaultValue;
        } else {
            // Leave the placeholder as-is if no value and no default.
            replacement = m.captured(0);
        }

        result.replace(m.capturedStart(), m.capturedLength(), replacement);
    }

    return result;
}

// ============================================================================
//  Cache management
// ============================================================================

void PromptManager::clearCache()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.clear();
}

void PromptManager::invalidate(const QString &name)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.remove(name);
}

int PromptManager::cacheSize() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cache.size();
}

// ============================================================================
//  Private helpers
// ============================================================================

QString PromptManager::readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString content = stream.readAll();
    file.close();
    return content;
}

QString PromptManager::resolvePath(const QString &name) const
{
    // Try .md first, then .txt.
    QString base;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        base = m_baseDir;
    }

    QStringList extensions = {QStringLiteral(".md"), QStringLiteral(".txt")};

    for (const auto &ext : extensions) {
        QString candidate = base + QStringLiteral("/") + name + ext;
        if (QFileInfo::exists(candidate)) {
            return QFileInfo(candidate).absoluteFilePath();
        }
    }

    return {};
}

} // namespace KatHub
