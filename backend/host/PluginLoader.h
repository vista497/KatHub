#pragma once

#include <QLibrary>
#include <QString>
#include <vector>

class IPlugin;
class PluginRegistry;

// Lightweight record for a loaded plugin DLL.
struct LoadedPlugin
{
    QString   dllPath;   // absolute path to the .dll
    QLibrary *library;   // owns the OS handle; nullptr when not loaded
    IPlugin  *plugin;    // pointer obtained from createPlugin()
};

// Loads / unloads / reloads plugin DLLs and keeps them registered in
// a thread-safe PluginRegistry.
class PluginLoader
{
public:
    explicit PluginLoader(PluginRegistry &registry);
    ~PluginLoader();

    // Scan a directory for *.dll files (returns absolute paths).
    static std::vector<QString> scanDirectory(const QString &dirPath);

    // Load a single DLL, call its createPlugin() factory, register it.
    // Pass an optional HostApi pointer for IPlugin::init().
    // Returns the IPlugin* on success, nullptr on failure.
    IPlugin *load(const QString &dllPath, void *hostApi = nullptr);

    // Graceful unload: mark draining, shutdown, unregister, unload.
    void unload(IPlugin *plugin);

    // Hot-reload: shutdown old → unload old DLL → load new DLL → init new.
    // Finds the existing plugin by name in the registry.
    // Returns the new IPlugin* on success, nullptr on failure.
    IPlugin *reload(const QString &pluginName, void *hostApi = nullptr);

    // Snapshot of currently tracked loaded plugins (no lock).
    const std::vector<LoadedPlugin> &loadedPlugins() const;

private:
    // Find a LoadedPlugin record by IPlugin pointer.
    LoadedPlugin *findRecord(IPlugin *plugin);
    const LoadedPlugin *findRecord(IPlugin *plugin) const;

    // Remove a record from the internal list (caller must handle
    // unregistration).
    void removeRecord(IPlugin *plugin);

    PluginRegistry &registry_;
    std::vector<LoadedPlugin> loaded_;
};
