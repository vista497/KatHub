#include "PluginLoader.h"
#include "PluginRegistry.h"
#include "IPlugin.h"

#include <QDir>
#include <QFileInfo>
#include <algorithm>

// ============================================================================
//  Construction / destruction
// ============================================================================

PluginLoader::PluginLoader(PluginRegistry &registry)
    : registry_(registry) {}

PluginLoader::~PluginLoader()
{
    // Unload every plugin tracked by this loader.  Shutdown + unload.
    while (!loaded_.empty())
    {
        auto &record = loaded_.back();
        if (record.plugin)
            unload(record.plugin);
        else
            loaded_.pop_back(); // safety: remove orphan records
    }
}

// ============================================================================
//  scanDirectory
// ============================================================================

std::vector<QString> PluginLoader::scanDirectory(const QString &dirPath)
{
    std::vector<QString> dlls;
    QDir dir(dirPath);
    if (!dir.exists())
        return dlls;

    const auto entries = dir.entryInfoList({QStringLiteral("*.dll")},
                                           QDir::Files | QDir::Readable);
    dlls.reserve(entries.size());
    for (const auto &fi : entries)
        dlls.push_back(fi.absoluteFilePath());

    return dlls;
}

// ============================================================================
//  load
// ============================================================================

IPlugin *PluginLoader::load(const QString &dllPath, void *hostApi)
{
    // Prevent loading the same path twice.
    for (const auto &rec : loaded_)
    {
        if (rec.dllPath == dllPath)
            return rec.plugin; // already loaded
    }

    auto *lib = new QLibrary(dllPath);
    if (!lib->load())
    {
        delete lib;
        return nullptr;
    }

    // Look up the extern "C" factory.
    using FactoryFunc = IPlugin *(*)();
    auto factory = reinterpret_cast<FactoryFunc>(lib->resolve("createPlugin"));
    if (!factory)
    {
        lib->unload();
        delete lib;
        return nullptr;
    }

    IPlugin *plugin = factory();
    if (!plugin)
    {
        lib->unload();
        delete lib;
        return nullptr;
    }

    // Register in the thread-safe registry.
    registry_.registerPlugin(plugin);

    // Initialise the plugin if a host API was provided.
    if (hostApi)
        plugin->init(hostApi);

    // Track locally.
    loaded_.push_back({dllPath, lib, plugin});
    return plugin;
}

// ============================================================================
//  unload  (graceful)
// ============================================================================

void PluginLoader::unload(IPlugin *plugin)
{
    if (!plugin)
        return;

    const std::string name = plugin->name();

    // Mark as draining so new requests are rejected.
    registry_.markDraining(name);

    // Shut the plugin down.
    plugin->shutdown();

    // Unregister from registry.
    registry_.unregisterPlugin(name);
    registry_.clearDraining(name);

    // Unload the DLL and clean up.
    LoadedPlugin *rec = findRecord(plugin);
    if (rec)
    {
        if (rec->library)
        {
            rec->library->unload();
            delete rec->library;
            rec->library = nullptr;
        }
        rec->plugin = nullptr;
        removeRecord(plugin);
    }
}

// ============================================================================
//  reload  (hot-reload)
// ============================================================================

IPlugin *PluginLoader::reload(const QString &pluginName, void *hostApi)
{
    // Find the currently loaded plugin by name.
    std::string nameStd = pluginName.toStdString();
    IPlugin *oldPlugin = registry_.findByName(nameStd);
    if (!oldPlugin)
        return nullptr;

    // Find its DLL path before we unload.
    const LoadedPlugin *oldRec = findRecord(oldPlugin);
    if (!oldRec)
        return nullptr;

    const QString dllPath = oldRec->dllPath;

    // Graceful teardown of the old plugin.
    registry_.markDraining(nameStd);
    oldPlugin->shutdown();
    registry_.unregisterPlugin(nameStd);

    // Unload the old DLL and remove the record.
    if (oldRec->library)
    {
        oldRec->library->unload();
        delete oldRec->library;
    }
    removeRecord(oldPlugin); // invalidates oldRec

    // Load the new DLL from the same path.
    return load(dllPath, hostApi);
}

// ============================================================================
//  Accessors
// ============================================================================

const std::vector<LoadedPlugin> &PluginLoader::loadedPlugins() const
{
    return loaded_;
}

// ============================================================================
//  Internal helpers
// ============================================================================

LoadedPlugin *PluginLoader::findRecord(IPlugin *plugin)
{
    auto it = std::find_if(loaded_.begin(), loaded_.end(),
                           [plugin](const LoadedPlugin &r) {
                               return r.plugin == plugin;
                           });
    return (it != loaded_.end()) ? &(*it) : nullptr;
}

const LoadedPlugin *PluginLoader::findRecord(IPlugin *plugin) const
{
    auto it = std::find_if(loaded_.begin(), loaded_.end(),
                           [plugin](const LoadedPlugin &r) {
                               return r.plugin == plugin;
                           });
    return (it != loaded_.end()) ? &(*it) : nullptr;
}

void PluginLoader::removeRecord(IPlugin *plugin)
{
    loaded_.erase(
        std::remove_if(loaded_.begin(), loaded_.end(),
                        [plugin](const LoadedPlugin &r) {
                            return r.plugin == plugin;
                        }),
        loaded_.end());
}
