#include <kathub/backend/BackendLoader.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcessEnvironment>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <regex>
#include <sstream>

// For PluginLoader integration (optional dependency).
// Include is guarded — the header is only included if PluginLoader is
// available in the consuming target's include path.
#if __has_include("PluginLoader.h")
    #include "PluginLoader.h"
    #include "IPlugin.h"
    #define KATHUB_HAS_PLUGIN_LOADER 1
#else
    #define KATHUB_HAS_PLUGIN_LOADER 0
#endif

namespace kathub {
namespace backend {

// ============================================================================
//  Static factory registry
// ============================================================================

std::unordered_map<std::string, BackendProviderFactory> &
BackendLoader::factories()
{
    static std::unordered_map<std::string, BackendProviderFactory> map;
    return map;
}

void BackendLoader::registerFactory(const std::string &type,
                                    BackendProviderFactory factory)
{
    factories()[type] = std::move(factory);
}

// ============================================================================
//  Construction / Destruction
// ============================================================================

BackendLoader::BackendLoader(QObject *parent)
    : QObject(parent)
{
}

BackendLoader::~BackendLoader()
{
    unloadAll();
}

// ============================================================================
//  PluginLoader integration
// ============================================================================

void BackendLoader::setPluginLoader(PluginLoader *loader)
{
    pluginLoader_ = loader;
}

// ============================================================================
//  Env var resolution
// ============================================================================

std::string BackendLoader::resolveEnvVar(const std::string &raw)
{
    // Pattern: ${VAR_NAME} or $VAR_NAME
    static const std::regex envRegex(R"(\$\{([A-Za-z_][A-Za-z0-9_]*)\})");
    static const std::regex envRegexShort(R"(\$([A-Za-z_][A-Za-z0-9_]*))");

    auto env = QProcessEnvironment::systemEnvironment();
    std::string result = raw;

    // First, resolve ${VAR} form.
    {
        std::smatch match;
        std::string tmp;
        while (std::regex_search(result, match, envRegex)) {
            const std::string varName = match[1].str();
            const QString qVal = env.value(QString::fromStdString(varName));
            const std::string val = qVal.isEmpty() && !env.contains(QString::fromStdString(varName))
                ? match[0].str()   // keep literal if not set
                : qVal.toStdString();
            tmp += match.prefix().str() + val;
            result = match.suffix().str();
        }
        tmp += result;
        result = std::move(tmp);
    }

    // Then resolve $VAR form (only if not preceded by backslash).
    {
        std::smatch match;
        std::string tmp;
        std::string subject = result;
        while (std::regex_search(subject, match, envRegexShort)) {
            // Check if the $ is escaped.
            if (match.prefix().length() > 0
                && match.prefix().str().back() == '\\') {
                // Escaped — keep literal, strip backslash.
                tmp += match.prefix().str().substr(0, match.prefix().length() - 1)
                    + match[0].str();
                subject = match.suffix().str();
                continue;
            }
            const std::string varName = match[1].str();
            const QString qVal = env.value(QString::fromStdString(varName));
            const std::string val = qVal.isEmpty() && !env.contains(QString::fromStdString(varName))
                ? match[0].str()
                : qVal.toStdString();
            tmp += match.prefix().str() + val;
            subject = match.suffix().str();
        }
        tmp += subject;
        result = std::move(tmp);
    }

    return result;
}

// ============================================================================
//  Parsing
// ============================================================================

std::optional<ProviderConfig>
BackendLoader::parseProviderEntry(const QJsonObject &obj, int index) const
{
    ProviderConfig cfg;

    // --- name (required) ---
    if (!obj.contains(QStringLiteral("name"))) {
        std::ostringstream oss;
        oss << "providers[" << index << "]: missing required field \"name\"";
        lastError_ = oss.str();
        return std::nullopt;
    }
    cfg.name = obj.value(QStringLiteral("name")).toString().toStdString();
    if (cfg.name.empty()) {
        std::ostringstream oss;
        oss << "providers[" << index << "]: \"name\" must not be empty";
        lastError_ = oss.str();
        return std::nullopt;
    }

    // --- type (required) ---
    if (!obj.contains(QStringLiteral("type"))) {
        std::ostringstream oss;
        oss << "providers[" << index << "] (\"" << cfg.name
            << "\"): missing required field \"type\"";
        lastError_ = oss.str();
        return std::nullopt;
    }
    cfg.type = obj.value(QStringLiteral("type")).toString().toStdString();

    // --- endpoint (optional) ---
    if (obj.contains(QStringLiteral("endpoint"))) {
        cfg.endpoint = obj.value(QStringLiteral("endpoint")).toString().toStdString();
    }

    // --- api_key with env-var resolution ---
    if (obj.contains(QStringLiteral("api_key"))) {
        cfg.apiKeyEnv = obj.value(QStringLiteral("api_key")).toString().toStdString();
        cfg.apiKey = resolveEnvVar(cfg.apiKeyEnv);
    }

    // --- model (optional) ---
    if (obj.contains(QStringLiteral("model"))) {
        cfg.model = obj.value(QStringLiteral("model")).toString().toStdString();
    }

    // --- parameters (optional, object) ---
    if (obj.contains(QStringLiteral("parameters"))) {
        QJsonValue pv = obj.value(QStringLiteral("parameters"));
        if (!pv.isObject()) {
            std::ostringstream oss;
            oss << "providers[" << index << "] (\"" << cfg.name
                << "\"): \"parameters\" must be an object";
            lastError_ = oss.str();
            return std::nullopt;
        }
        cfg.parameters = pv.toObject();
    }

    // --- priority (optional, for ordering) ---
    // stored in parameters for downstream use

    return cfg;
}

// ============================================================================
//  Provider instantiation
// ============================================================================

kathub::ai::IBackendProvider *
BackendLoader::instantiateProvider(const ProviderConfig &cfg)
{
    // 1. Try built-in factory.
    auto &f = factories();
    auto it = f.find(cfg.type);
    if (it != f.end()) {
        auto provider = it->second(cfg);
        if (provider) {
            std::cout << "BackendLoader: loaded built-in provider \""
                      << cfg.name << "\" (type=" << cfg.type
                      << ", model=" << cfg.model << ")" << std::endl;
            return provider.release();
        }
        std::cerr << "BackendLoader: built-in factory for type \""
                  << cfg.type << "\" returned nullptr for provider \""
                  << cfg.name << "\"" << std::endl;
        return nullptr;
    }

    // 2. Try PluginLoader (DLL-based backend).
#if KATHUB_HAS_PLUGIN_LOADER
    if (pluginLoader_) {
        // Search for <type>_backend.dll in known plugin directories.
        // We try: ./plugins/<type>_backend.dll, ./<type>_backend.dll
        static const std::vector<QString> searchDirs = {
            QDir::currentPath() + QStringLiteral("/plugins"),
            QCoreApplication::applicationDirPath() + QStringLiteral("/plugins"),
            QDir::currentPath(),
        };

        const QString dllName = QString::fromStdString(cfg.type)
                                + QStringLiteral("_backend.dll");
        QString foundPath;

        for (const QString &dir : searchDirs) {
            QString candidate = dir + QStringLiteral("/") + dllName;
            if (QFileInfo::exists(candidate)) {
                foundPath = candidate;
                break;
            }
        }

        if (!foundPath.isEmpty()) {
            IPlugin *plugin = pluginLoader_->load(foundPath, nullptr);
            if (plugin) {
                // Attempt to cast to IBackendProvider via dynamic_cast.
                auto *backend = dynamic_cast<kathub::ai::IBackendProvider *>(plugin);
                if (backend) {
                    // Build initialization config JSON.
                    QJsonObject configJson;
                    configJson[QStringLiteral("endpoint")] = QString::fromStdString(cfg.endpoint);
                    configJson[QStringLiteral("api_key")]  = QString::fromStdString(cfg.apiKey);
                    configJson[QStringLiteral("model")]    = QString::fromStdString(cfg.model);
                    configJson[QStringLiteral("parameters")] = cfg.parameters;
                    QJsonDocument doc(configJson);

                    if (backend->initialize(doc.toJson(QJsonDocument::Compact).toStdString())) {
                        std::cout << "BackendLoader: loaded DLL provider \""
                                  << cfg.name << "\" (type=" << cfg.type
                                  << ", dll=" << foundPath.toStdString() << ")"
                                  << std::endl;
                        return backend;
                    }
                    std::cerr << "BackendLoader: DLL provider \""
                              << cfg.name << "\" failed to initialize"
                              << std::endl;
                    // PluginLoader owns the IPlugin; unload it.
                    pluginLoader_->unload(plugin);
                    return nullptr;
                }
                std::cerr << "BackendLoader: DLL provider \""
                          << cfg.name << "\" does not implement IBackendProvider"
                          << std::endl;
                pluginLoader_->unload(plugin);
                return nullptr;
            }
            std::cerr << "BackendLoader: failed to load DLL \""
                      << foundPath.toStdString() << "\"" << std::endl;
        }
    }
#endif

    // 3. No factory found.
    std::cerr << "BackendLoader: no factory or DLL found for type \""
              << cfg.type << "\" (provider \"" << cfg.name << "\")"
              << std::endl;
    return nullptr;
}

// ============================================================================
//  loadImpl — core loading logic (shared by load + hot-reload)
// ============================================================================

bool BackendLoader::loadImpl(const std::string &path)
{
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::ostringstream oss;
        oss << "Cannot open providers config: " << path
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
        lastError_ = "providers.json root must be a JSON object";
        return false;
    }

    QJsonObject root = doc.object();

    // "providers" array is required.
    if (!root.contains(QStringLiteral("providers"))) {
        lastError_ = "providers.json must contain a \"providers\" array";
        return false;
    }

    QJsonValue providersVal = root.value(QStringLiteral("providers"));
    if (!providersVal.isArray()) {
        lastError_ = "\"providers\" must be an array";
        return false;
    }

    QJsonArray providersArr = providersVal.toArray();

    // Unload any previously loaded providers.
    unloadAll();

    // Parse and instantiate each entry.
    int loadedCount = 0;
    for (int i = 0; i < providersArr.size(); ++i) {
        QJsonValue entryVal = providersArr[i];
        if (!entryVal.isObject()) {
            std::ostringstream oss;
            oss << "providers[" << i << "]: must be an object, got "
                << (entryVal.isString()  ? "string"
                    : entryVal.isDouble() ? "number"
                    : entryVal.isBool()   ? "boolean"
                    : entryVal.isArray()  ? "array"
                    : entryVal.isNull()   ? "null"
                    :                       "unknown");
            lastError_ = oss.str();
            unloadAll();
            return false;
        }

        auto cfgOpt = parseProviderEntry(entryVal.toObject(), i);
        if (!cfgOpt.has_value()) {
            unloadAll();
            return false;
        }

        // Check for duplicate names.
        if (providers_.count(cfgOpt->name)) {
            std::ostringstream oss;
            oss << "providers[" << i << "]: duplicate provider name \""
                << cfgOpt->name << "\"";
            lastError_ = oss.str();
            unloadAll();
            return false;
        }

        auto *p = instantiateProvider(*cfgOpt);
        if (p) {
            providers_[cfgOpt->name] = std::unique_ptr<kathub::ai::IBackendProvider>(p);
            ++loadedCount;
        } else {
            // Non-fatal for individual provider failures:
            // log and continue, but warn.
            std::cerr << "BackendLoader: skipping provider \""
                      << cfgOpt->name << "\" — instantiation failed"
                      << std::endl;
        }
    }

    filePath_ = path;
    lastError_.clear();

    std::cout << "BackendLoader: loaded " << loadedCount << " provider(s) from "
              << path << std::endl;

    emit providersChanged();
    return true;
}

// ============================================================================
//  loadFromJson
// ============================================================================

bool BackendLoader::loadFromJson(const std::string &path)
{
    return loadImpl(path);
}

// ============================================================================
//  reload
// ============================================================================

bool BackendLoader::reload()
{
    if (filePath_.empty()) {
        lastError_ = "No file loaded yet — nothing to reload";
        return false;
    }
    return loadImpl(filePath_);
}

// ============================================================================
//  unloadAll
// ============================================================================

void BackendLoader::unloadAll()
{
    providers_.clear();
}

// ============================================================================
//  Hot-reload
// ============================================================================

void BackendLoader::enableHotReload(bool enable)
{
    if (enable) {
        if (filePath_.empty()) {
            std::cerr << "BackendLoader: cannot enable hot-reload — no file loaded yet"
                      << std::endl;
            return;
        }

        if (!fileWatcher_) {
            fileWatcher_ = std::make_unique<QFileSystemWatcher>(this);
            connect(fileWatcher_.get(), &QFileSystemWatcher::fileChanged,
                    this, [this](const QString &changedPath) {
                        std::cout << "BackendLoader: config changed — reloading "
                                  << changedPath.toStdString() << std::endl;

                        if (!loadImpl(changedPath.toStdString())) {
                            std::cerr << "BackendLoader: hot-reload failed: "
                                      << lastError_ << std::endl;
                            emit providerLoadFailed(
                                QString::fromStdString(filePath_),
                                QString::fromStdString(lastError_));
                        }

                        // Re-add the file to the watcher (some editors
                        // replace the file, which removes the watch).
                        if (fileWatcher_ && !fileWatcher_->files().contains(changedPath)) {
                            fileWatcher_->addPath(changedPath);
                        }
                    });
        }

        if (!fileWatcher_->files().contains(QString::fromStdString(filePath_))) {
            fileWatcher_->addPath(QString::fromStdString(filePath_));
        }
        std::cout << "BackendLoader: hot-reload enabled for " << filePath_ << std::endl;
    } else {
        if (fileWatcher_) {
            if (!fileWatcher_->files().isEmpty()) {
                fileWatcher_->removePaths(fileWatcher_->files());
            }
        }
        std::cout << "BackendLoader: hot-reload disabled" << std::endl;
    }
}

bool BackendLoader::isHotReloadEnabled() const
{
    return fileWatcher_ && !fileWatcher_->files().isEmpty();
}

// ============================================================================
//  Provider access
// ============================================================================

kathub::ai::IBackendProvider *
BackendLoader::provider(const std::string &name) const
{
    auto it = providers_.find(name);
    return (it != providers_.end()) ? it->second.get() : nullptr;
}

std::vector<kathub::ai::IBackendProvider *>
BackendLoader::allProviders() const
{
    std::vector<kathub::ai::IBackendProvider *> result;
    result.reserve(providers_.size());
    for (const auto &pair : providers_) {
        result.push_back(pair.second.get());
    }
    return result;
}

size_t BackendLoader::providerCount() const
{
    return providers_.size();
}

} // namespace backend
} // namespace kathub
