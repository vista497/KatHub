#include "test_plugin.h"

#include <string>

// ---- IPlugin ----

const char *TestPlugin::name()    { return "TestPlugin"; }
const char *TestPlugin::version() { return "1.0.0"; }

bool TestPlugin::init(void * /*hostApi*/)
{
    // Nothing to initialise in this minimal test plugin.
    return true;
}

void TestPlugin::shutdown()
{
    // Nothing to tear down.
}

// ---- IBackendProvider ----

bool TestPlugin::initialize(const std::string &config)
{
    // Store the config string for verification in tests.
    lastConfig_ = config;
    initialized_ = true;
    return true;
}

std::string TestPlugin::chat(const std::string &prompt)
{
    return "mock response to: " + prompt;
}

std::string TestPlugin::getCapabilities()
{
    return "chat,mock";
}

std::string TestPlugin::getName()
{
    return "TestBackend";
}

// ---- DLL factory ----

extern "C" IPlugin *createPlugin()
{
    return new TestPlugin();
}
