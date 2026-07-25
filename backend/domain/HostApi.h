#pragma once

// Concrete struct passed to plugins during IPlugin::init().
// All service pointers are opaque (void*) to avoid ABI coupling
// between the host executable and plugin DLLs.

struct HostApi
{
    // Opaque pointer to the Router — plugins call back through this
    // to register routes or IHttpHandler instances.
    void* router;

    // Opaque pointer to the EventBus — plugins use this to publish
    // or subscribe to system-wide events.
    void* eventBus;

    // Opaque pointer to the ConfigService — plugins read
    // configuration values through this.
    void* configService;

    // Opaque pointer to the LogService — plugins write log
    // messages through this.
    void* logService;

    // Version string of the KatHub host (e.g. "0.1.0").
    const char* hostVersion;
};
