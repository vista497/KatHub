#pragma once

#include "IPlugin.h"

// Minimal IPlugin implementation used to verify the plugin loader.
class TestPlugin : public IPlugin
{
public:
    const char *name() override;
    const char *version() override;
    bool init(void *hostApi) override;
    void shutdown() override;
};
