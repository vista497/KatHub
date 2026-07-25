#include "test_plugin.h"

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

// DLL factory – exported so PluginLoader can resolve it.
extern "C" IPlugin *createPlugin()
{
    return new TestPlugin();
}
