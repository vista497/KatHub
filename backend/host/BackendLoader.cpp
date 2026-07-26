#include "BackendLoader.h"
#include "IPlugin.h"
#include "PluginLoader.h"

#include "kathub/ai/IBackendProvider.h"
#include "OpenRouterClient.h"

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

#include <chrono>
#include <filesystem>
#include <iostream>
#include <regex>
#include <sstream>

// Register built-in OpenRouter backend provider
REGISTER_BACKEND("openrouter", KatHub::OpenRouterClient)

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

BackendLoader::BackendLoader(PluginLoader &loader, QObject *parent)
    : QObject(parent), loader_(loader)
{
}

BackendLoader::~BackendLoader()
{
    unloadAll();
}

// ============================================================================
//  Env var resolution
// ============================================================================

std::string BackendLoader::resolveEnvVar(const std::string &raw)
{
    static const std::regex envRegex(R"(\$\{([A-Za-z_][A-Za-z0-9_]*)\})");

    auto env = QProcessEnvironment::systemEnvironment();
    std::string result = raw;

    std::smatch match;
    std::string tmp;
    while (std::regex_search(result, match, envRegex)) {
        const std::string varName = match[1].str();
        const QString qVal =
            env.value(QString::fromStdString(varName));
        const std::string val =
            qVal.isEmpty() && !env.contains(QString::fromStdString(varName))
                ? match[0].str()
                : qVal.toStdString();
        tmp += match.prefix().str() + val;
        result = match.suffix().str();
    }
    tmp += result;
    return tmp;
}

// ============================================================================
//  Parsing
// ============================================================================

std::optional<ProviderConfig>
BackendLoader::parseProviderEntry(const QJsonObject &obj, int index)
{
    ProviderConfig cfg;

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

    bool hasType = obj.contains(QStringLiteral("type"));
    bool hasDll  = obj.contains(QStringLiteral("dll"));

    if (hasType) {
        cfg.isPhase2 = true;
        cfg.type = obj.value(QStringLiteral("type")).toString().toStdString();

        if (obj.contains(QStringLiteral("endpoint"))) {
            cfg.endpoint =
                obj.value(QStringLiteral("endpoint")).toString().toStdString();
        }

        if (obj.contains(QStringLiteral("api_key"))) {
            cfg.apiKeyEnv =
                obj.value(QStringLiteral("api_key")).toString().toStdString();
            cfg.apiKey = resolveEnvVar(cfg.apiKeyEnv);
        }

        if (obj.contains(QStringLiteral("model"))) {
            cfg.model =
                obj.value(QStringLiteral("model")).toString().toStdString();
        }

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
    } else if (hasDll) {
        cfg.isPhase2 = false;
        cfg.dllPath =
            obj.value(QStringLiteral("dll")).toString().toStdString();

        if (obj.contains(QStringLiteral("config"))) {
            cfg.configStr =
                obj.value(QStringLiteral("config")).toString().toStdString();
        }

        if (obj.contains(QStringLiteral("type"))) {
            cfg.type =
                obj.value(QStringLiteral("type")).toString().toStdString();
        }
    } else {
        std::ostringstream oss;
        oss << "providers[" << index << "] (\"" << cfg.name
            << "\"): must contain either \"type\" (Phase 2) or \"dll\" (Phase 1)";
        lastError_ = oss.str();
        return std::nullopt;
    }

    return cfg;
}

// ============================================================================
//  Provider instantiation -- DLL path (Phase 1)
// ============================================================================

kathub::ai::IBackendProvider *
BackendLoader::instantiateFromDll(const ProviderConfig &cfg)
{
    QString dllPath = QString::fromStdString(cfg.dllPath);

    if (dllPath.isEmpty()) {
        std::cerr << "BackendLoader: empty DLL path for provider \""
                  << cfg.name << "\"" << std::endl;
        return nullptr;
    }

    IPlugin *plugin = loader_.load(dllPath, nullptr);
    if (!plugin) {
        std::cerr << "BackendLoader: failed to load plugin DLL: "
                  << dllPath.toStdString() << std::endl;
        return nullptr;
    }

    auto *provider = dynamic_cast<kathub::ai::IBackendProvider *>(plugin);
    if (!provider) {
        std::cerr << "BackendLoader: plugin " << cfg.name
                  << " does not implement IBackendProvider" << std::endl;
        loader_.unload(plugin);
        return nullptr;
    }

    std::string configToUse = cfg.configStr.empty() ? "{}" : cfg.configStr;
    if (!provider->initialize(configToUse)) {
        std::cerr << "BackendLoader: provider \"" << cfg.name
                  << "\" failed to initialize" << std::endl;
        loader_.unload(plugin);
        return nullptr;
    }

    std::cout << "BackendLoader: loaded DLL provider \""
              << cfg.name << "\" from " << dllPath.toStdString() << std::endl;
    return provider;
}

// ============================================================================
//  Provider instantiation -- factory (Phase 2)
// ============================================================================

kathub::ai::IBackendProvider *
BackendLoader::instantiateFromFactory(const ProviderConfig &cfg)
{
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

    if (!cfg.type.empty()) {
        static const std::vector<QString> searchDirs = {
            QDir::currentPath() + QStringLiteral("/plugins"),
            QCoreApplication::applicationDirPath() + QStringLiteral("/plugins"),
            QDir::currentPath(),
        };

        const QString dllName =
            QString::fromStdString(cfg.type) + QStringLiteral("_backend.dll");
        QString foundPath;

        for (const QString &dir : searchDirs) {
            QString candidate = dir + QStringLiteral("/") + dllName;
            if (QFileInfo::exists(candidate)) {
                foundPath = candidate;
                break;
            }
        }

        if (!foundPath.isEmpty()) {
            IPlugin *plugin = loader_.load(foundPath, nullptr);
            if (plugin) {
                auto *provider =
                    dynamic_cast<kathub::ai::IBackendProvider *>(plugin);
                if (provider) {
                    QJsonObject configJson;
                    configJson[QStringLiteral("endpoint")] =
                        QString::fromStdString(cfg.endpoint);
                    configJson[QStringLiteral("api_key")] =
                        QString::fromStdString(cfg.apiKey);
                    configJson[QStringLiteral("model")] =
                        QString::fromStdString(cfg.model);
                    configJson[QStringLiteral("parameters")] = cfg.parameters;
                    QJsonDocument doc(configJson);

                    if (provider->initialize(
                            doc.toJson(QJsonDocument::Compact).toStdString())) {
                        std::cout
                            << "BackendLoader: loaded DLL provider \""
                            << cfg.name << "\" (type=" << cfg.type
                            << ", dll=" << foundPath.toStdString() << ")"
                            << std::endl;
                        return provider;
                    }
                    std::cerr << "BackendLoader: DLL provider \""
                              << cfg.name << "\" failed to initialize"
                              << std::endl;
                    loader_.unload(plugin);
                    return nullptr;
                }
                std::cerr << "BackendLoader: DLL provider \""
                          << cfg.name
                          << "\" does not implement IBackendProvider"
                          << std::endl;
                loader_.unload(plugin);
                return nullptr;
            }
            std::cerr << "BackendLoader: failed to load DLL \""
                      << foundPath.toStdString() << "\"" << std::endl;
        }
    }

    std::cerr << "BackendLoader: no factory or DLL found for type \""
              << cfg.type << "\" (provider \"" << cfg.name << "\")"
              << std::endl;
    return nullptr;
}

// ============================================================================
//  Provider instantiation -- dispatch
// ============================================================================

kathub::ai::IBackendProvider *
BackendLoader::instantiateProvider(const ProviderConfig &cfg)
{
    if (cfg.isPhase2) {
        return instantiateFromFactory(cfg);
    } else {
        if (!cfg.type.empty()) {
            auto *p = instantiateFromFactory(cfg);
            if (p) return p;
        }
        return instantiateFromDll(cfg);
    }
}

// ============================================================================
//  loadImpl
// ============================================================================

std::vector<kathub::ai::IBackendProvider *>
BackendLoader::loadImpl(const std::string &path)
{
    providers_.clear();
    lastError_.clear();
    isLoaded_ = false;

    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::ostringstream oss;
        oss << "Cannot open providers file: " << path
            << " (" << file.errorString().toStdString() << ")";
        lastError_ = oss.str();
        std::cerr << "BackendLoader: " << lastError_ << std::endl;
        return {};
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        std::ostringstream oss;
        oss << "JSON parse error in " << path << " at offset "
            << parseError.offset << ": "
            << parseError.errorString().toStdString();
        lastError_ = oss.str();
        std::cerr << "BackendLoader: " << lastError_ << std::endl;
        return {};
    }

    if (!doc.isObject()) {
        lastError_ = "providers.json root must be a JSON object";
        std::cerr << "BackendLoader: " << lastError_ << std::endl;
        return {};
    }

    QJsonObject root = doc.object();

    if (!root.contains(QStringLiteral("providers"))) {
        lastError_ = "providers.json missing 'providers' key";
        std::cerr << "BackendLoader: " << lastError_ << std::endl;
        return {};
    }

    QJsonValue providersVal = root.value(QStringLiteral("providers"));
    if (!providersVal.isArray()) {
        lastError_ = "'providers' must be a JSON array";
        std::cerr << "BackendLoader: " << lastError_ << std::endl;
        return {};
    }

    QJsonArray providersArr = providersVal.toArray();

    int loadedCount = 0;
    for (int i = 0; i < providersArr.size(); ++i) {
        QJsonValue entryVal = providersArr[i];
        if (!entryVal.isObject()) {
            std::cerr
                << "BackendLoader: skipping non-object provider entry at index "
                << i << std::endl;
            continue;
        }

        auto cfgOpt = parseProviderEntry(entryVal.toObject(), i);
        if (!cfgOpt.has_value()) {
            std::cerr << "BackendLoader: " << lastError_ << std::endl;
            continue;
        }

        auto *p = instantiateProvider(*cfgOpt);
        if (p) {
            providers_.push_back(p);
            ++loadedCount;
        }
    }

    try {
        lastWriteTime_ = std::filesystem::last_write_time(path);
        hasLastWriteTime_ = true;
    } catch (const std::filesystem::filesystem_error &) {
        hasLastWriteTime_ = false;
    }

    filePath_ = path;
    isLoaded_ = true;

    if (loadedCount > 0 || providersArr.isEmpty()) {
        lastError_.clear();
    }

    std::cout << "BackendLoader: loaded " << loadedCount << " provider(s) from "
              << path << std::endl;

    emit providersChanged();
    return providers_;
}

std::vector<kathub::ai::IBackendProvider *>
BackendLoader::loadFromFile(const std::string &path)
{
    return loadImpl(path);
}

std::vector<kathub::ai::IBackendProvider *>
BackendLoader::reload()
{
    if (filePath_.empty()) {
        lastError_ = "No file loaded yet -- nothing to reload";
        return {};
    }
    return loadImpl(filePath_);
}

bool BackendLoader::needsReload(const std::string &path) const
{
    if (!hasLastWriteTime_)
        return false;

    try {
        auto current = std::filesystem::last_write_time(path);
        return current > lastWriteTime_;
    } catch (const std::filesystem::filesystem_error &) {
        return isLoaded_;
    }
}

void BackendLoader::enableHotReload(bool enable)
{
    if (enable) {
        if (filePath_.empty()) {
            std::cerr << "BackendLoader: cannot enable hot-reload -- "
                         "no file loaded yet"
                      << std::endl;
            return;
        }

        if (!fileWatcher_) {
            fileWatcher_ = std::make_unique<QFileSystemWatcher>(this);
            connect(fileWatcher_.get(), &QFileSystemWatcher::fileChanged,
                    this, [this](const QString &changedPath) {
                        std::cout
                            << "BackendLoader: config changed -- reloading "
                            << changedPath.toStdString() << std::endl;

                        auto result = loadImpl(changedPath.toStdString());
                        if (result.empty() && !isLoaded_) {
                            std::cerr << "BackendLoader: hot-reload failed: "
                                      << lastError_ << std::endl;
                            emit providerLoadFailed(
                                QString::fromStdString(filePath_),
                                QString::fromStdString(lastError_));
                        }

                        if (fileWatcher_
                            && !fileWatcher_->files().contains(changedPath)) {
                            fileWatcher_->addPath(changedPath);
                        }
                    });
        }

        if (!fileWatcher_->files().contains(
                QString::fromStdString(filePath_))) {
            fileWatcher_->addPath(QString::fromStdString(filePath_));
        }
        std::cout << "BackendLoader: hot-reload enabled for " << filePath_
                  << std::endl;
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

kathub::ai::IBackendProvider *
BackendLoader::provider(const std::string &name) const
{
    for (auto *p : providers_) {
        if (p->getName() == name)
            return p;
    }
    return nullptr;
}

void BackendLoader::unloadAll()
{
    providers_.clear();
}
