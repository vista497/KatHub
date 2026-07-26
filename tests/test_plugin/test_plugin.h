#pragma once

#include "IPlugin.h"
#include "kathub/ai/IBackendProvider.h"

// Minimal IPlugin + IBackendProvider implementation used to verify the
// plugin loader and BackendLoader.
class TestPlugin : public IPlugin, public kathub::ai::IBackendProvider
{
public:
    // ---- IPlugin ----
    const char *name() override;
    const char *version() override;
    bool init(void *hostApi) override;
    void shutdown() override;

    // ---- IBackendProvider ----
    bool initialize(const std::string &config) override;
    std::string chat(const std::string &prompt) override;
    std::string getCapabilities() override;
    std::string getName() override;

    // Public for test inspection.
    std::string lastConfig_;
    bool initialized_ = false;
};
