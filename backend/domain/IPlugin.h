#pragma once

#include "domain_export.h"

// Pure abstract plugin interface.
// Plugins implement this and export a factory via extern "C".
class KATHUB_DOMAIN_EXPORT IPlugin
{
public:
    virtual ~IPlugin() = default;

    // Human-readable plugin name (e.g. "KatHub HTTP Router").
    virtual const char* name() = 0;

    // Semantic version string (e.g. "1.0.0").
    virtual const char* version() = 0;

    // Called once after the DLL is loaded.
    // hostApi is a pointer to a HostApi struct (see HostApi.h).
    // Returns true on success, false if initialization fails.
    virtual bool init(void* hostApi) = 0;

    // Called before the DLL is unloaded.
    virtual void shutdown() = 0;
};

// Factory function that every plugin DLL must export.
// The host calls this to obtain an IPlugin instance.
extern "C" KATHUB_DOMAIN_EXPORT IPlugin* createPlugin();
