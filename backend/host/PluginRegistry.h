#pragma once

#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class IPlugin;
class IHttpHandler;

// Thread-safe registry for plugins and HTTP handlers.
// Read methods acquire a shared_lock; write methods acquire a unique_lock.
class PluginRegistry
{
public:
    static PluginRegistry &instance();

    // ---- Plugin management ----

    void registerPlugin(IPlugin *plugin);
    void unregisterPlugin(const std::string &name);

    IPlugin *findByName(const std::string &name) const;
    std::vector<IPlugin *> allPlugins() const;

    // ---- HTTP handler management ----

    void registerHandler(IHttpHandler *handler);
    std::vector<IHttpHandler *> handlers() const;

    // ---- Draining support (graceful shutdown) ----

    void markDraining(const std::string &name);
    bool isDraining(const std::string &name) const;
    void clearDraining(const std::string &name);

private:
    PluginRegistry() = default;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, IPlugin *> plugins_;
    std::vector<IHttpHandler *> httpHandlers_;
    std::unordered_set<std::string> draining_;
};

// ---------------------------------------------------------------------------
//  Static handler registry – independent from PluginRegistry's dynamic
//  plugin registry.  REGISTER_HANDLER(ClassName) in a .cpp file creates a
//  static global whose constructor runs before main(), auto-registering the
//  handler here.
// ---------------------------------------------------------------------------
class StaticHandlerRegistry
{
public:
    static StaticHandlerRegistry &instance();

    void registerHandler(IHttpHandler *handler);
    const std::vector<IHttpHandler *> &handlers() const;

private:
    StaticHandlerRegistry() = default;
    std::vector<IHttpHandler *> handlers_;
};

// Macro – place in the .cpp file of the handler class:
//
//   #include "PluginRegistry.h"
//   REGISTER_HANDLER(MyHandler)
//
#define REGISTER_HANDLER(ClassName)                                          \
    namespace                                                                \
    {                                                                        \
        struct Register_##ClassName                                          \
        {                                                                    \
            Register_##ClassName()                                           \
            {                                                                \
                StaticHandlerRegistry::instance().registerHandler(           \
                    new ClassName());                                        \
            }                                                                \
        };                                                                   \
        static Register_##ClassName _kathub_register_##ClassName;            \
    }
