#include <gtest/gtest.h>
#include <QLibrary>
#include <QString>

#include "IPlugin.h"
#include "HostApi.h"

// TEST_PLUGIN_DLL_PATH is set by CMake to the absolute path of the test_plugin
// DLL (via $<TARGET_FILE:test_plugin>). The quoting in CMake ensures it arrives
// here as a string literal.
#ifndef TEST_PLUGIN_DLL_PATH
#error "TEST_PLUGIN_DLL_PATH must be defined by the build system"
#endif

class PluginLifecycleTest : public ::testing::Test
{
protected:
    const QString dllPath{TEST_PLUGIN_DLL_PATH};
};

// ---------------------------------------------------------------------------
// Verify that the plugin DLL can be loaded and that createPlugin() returns an
// IPlugin with the expected metadata.
// ---------------------------------------------------------------------------
TEST_F(PluginLifecycleTest, LoadPluginAndVerifyMetadata)
{
    QLibrary lib(dllPath);
    ASSERT_TRUE(lib.load())
        << "Failed to load plugin DLL: " << dllPath.toStdString();

    using CreatePluginFunc = IPlugin *(*)();
    auto createPlugin = reinterpret_cast<CreatePluginFunc>(
        lib.resolve("createPlugin"));
    ASSERT_NE(createPlugin, nullptr)
        << "Failed to resolve createPlugin symbol";

    IPlugin *plugin = createPlugin();
    ASSERT_NE(plugin, nullptr) << "createPlugin returned nullptr";

    EXPECT_STREQ(plugin->name(), "TestPlugin");
    EXPECT_STREQ(plugin->version(), "1.0.0");

    delete plugin;
    lib.unload();
}

// ---------------------------------------------------------------------------
// Verify that IPlugin::init() accepts a HostApi pointer and returns true.
// ---------------------------------------------------------------------------
TEST_F(PluginLifecycleTest, InitReturnsTrue)
{
    QLibrary lib(dllPath);
    ASSERT_TRUE(lib.load());

    using CreatePluginFunc = IPlugin *(*)();
    auto createPlugin = reinterpret_cast<CreatePluginFunc>(
        lib.resolve("createPlugin"));
    ASSERT_NE(createPlugin, nullptr);

    IPlugin *plugin = createPlugin();
    ASSERT_NE(plugin, nullptr);

    HostApi hostApi{};
    hostApi.hostVersion = "0.1.0";
    EXPECT_TRUE(plugin->init(&hostApi));

    delete plugin;
    lib.unload();
}

// ---------------------------------------------------------------------------
// Verify that IPlugin::shutdown() can be called after init() without
// crashing or throwing an exception.
// ---------------------------------------------------------------------------
TEST_F(PluginLifecycleTest, ShutdownDoesNotCrash)
{
    QLibrary lib(dllPath);
    ASSERT_TRUE(lib.load());

    using CreatePluginFunc = IPlugin *(*)();
    auto createPlugin = reinterpret_cast<CreatePluginFunc>(
        lib.resolve("createPlugin"));
    ASSERT_NE(createPlugin, nullptr);

    IPlugin *plugin = createPlugin();
    ASSERT_NE(plugin, nullptr);

    HostApi hostApi{};
    plugin->init(&hostApi);

    // Should not crash or throw
    EXPECT_NO_THROW(plugin->shutdown());

    delete plugin;
    lib.unload();
}
