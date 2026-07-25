#include <gtest/gtest.h>

#include "kathtech/config/JsonConfigLoader.h"

#include <fstream>
#include <cstdio>  // std::remove

using namespace KatHub;

// ============================================================================
//  Fixture — creates/cleans up a temporary config file per test.
// ============================================================================
class JsonConfigLoaderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tempPath_ = "test_kathub_config.json";
        // Ensure we start clean
        std::remove(tempPath_.c_str());
    }

    void TearDown() override
    {
        std::remove(tempPath_.c_str());
    }

    void writeFile(const std::string &content)
    {
        std::ofstream f(tempPath_);
        ASSERT_TRUE(f.is_open()) << "Failed to create temp config file";
        f << content;
        f.close();
    }

    std::string tempPath_;
};

// ============================================================================
//  Missing file → use defaults, load() returns false.
// ============================================================================
TEST_F(JsonConfigLoaderTest, MissingFileReturnsDefaults)
{
    JsonConfigLoader cfg("__nonexistent_file__.json");
    EXPECT_FALSE(cfg.load());
    EXPECT_FALSE(cfg.loaded());
    EXPECT_FALSE(cfg.lastError().empty());

    // All accessors should return defaults
    EXPECT_EQ(cfg.getString("server.host", "localhost"), "localhost");
    EXPECT_EQ(cfg.getInt("server.port", 8080), 8080);
    EXPECT_EQ(cfg.getBool("server.debug", true), true);
    EXPECT_TRUE(cfg.getValue("nested.key", nlohmann::json("fallback")).is_string());
    EXPECT_EQ(cfg.getValue("nested.key", nlohmann::json("fallback")).get<std::string>(),
              "fallback");
}

// ============================================================================
//  Malformed JSON → load() returns false, getters return defaults.
// ============================================================================
TEST_F(JsonConfigLoaderTest, MalformedJsonUsesDefaults)
{
    writeFile("{not valid json at all!!");

    JsonConfigLoader cfg(tempPath_);
    EXPECT_FALSE(cfg.load());
    EXPECT_FALSE(cfg.loaded());
    EXPECT_FALSE(cfg.lastError().empty());

    // Getters should still return defaults
    EXPECT_EQ(cfg.getString("anything", "default"), "default");
    EXPECT_EQ(cfg.getInt("anything", 42), 42);
}

// ============================================================================
//  Empty JSON file → load() succeeds, empty object is the data.
// ============================================================================
TEST_F(JsonConfigLoaderTest, EmptyJsonSucceeds)
{
    writeFile("{}");

    JsonConfigLoader cfg(tempPath_);
    EXPECT_TRUE(cfg.load());
    EXPECT_TRUE(cfg.loaded());
    EXPECT_TRUE(cfg.lastError().empty());

    EXPECT_TRUE(cfg.json().is_object());
    EXPECT_EQ(cfg.getString("nonexistent", "fallback"), "fallback");
}

// ============================================================================
//  Simple key-value read.
// ============================================================================
TEST_F(JsonConfigLoaderTest, SimpleKeyRead)
{
    writeFile(R"({"port": 9090, "host": "0.0.0.0", "debug": true})");

    JsonConfigLoader cfg(tempPath_);
    ASSERT_TRUE(cfg.load());

    EXPECT_EQ(cfg.getInt("port"), 9090);
    EXPECT_EQ(cfg.getString("host"), "0.0.0.0");
    EXPECT_EQ(cfg.getBool("debug"), true);
}

// ============================================================================
//  Nested key path (e.g. "server.port").
// ============================================================================
TEST_F(JsonConfigLoaderTest, NestedKeyPath)
{
    writeFile(R"({
        "server": {
            "port": 8080,
            "host": "127.0.0.1"
        },
        "logging": {
            "level": "debug",
            "file": "/var/log/kathub.log"
        }
    })");

    JsonConfigLoader cfg(tempPath_);
    ASSERT_TRUE(cfg.load());

    EXPECT_EQ(cfg.getInt("server.port"), 8080);
    EXPECT_EQ(cfg.getString("server.host"), "127.0.0.1");
    EXPECT_EQ(cfg.getString("logging.level"), "debug");
    EXPECT_EQ(cfg.getString("logging.file"), "/var/log/kathub.log");
}

// ============================================================================
//  Deep nesting (3+ levels).
// ============================================================================
TEST_F(JsonConfigLoaderTest, DeepNesting)
{
    writeFile(R"({
        "a": {
            "b": {
                "c": {
                    "value": 42
                }
            }
        }
    })");

    JsonConfigLoader cfg(tempPath_);
    ASSERT_TRUE(cfg.load());

    EXPECT_EQ(cfg.getInt("a.b.c.value"), 42);
}

// ============================================================================
//  Missing nested key → return default.
// ============================================================================
TEST_F(JsonConfigLoaderTest, MissingNestedKeyReturnsDefault)
{
    writeFile(R"({"server": {"port": 8080}})");

    JsonConfigLoader cfg(tempPath_);
    ASSERT_TRUE(cfg.load());

    EXPECT_EQ(cfg.getInt("server.host", -1), -1);
    EXPECT_EQ(cfg.getString("nonexistent.parent", "nope"), "nope");

    // Partially valid path should also return default
    EXPECT_EQ(cfg.getString("server.port.host", "invalid"), "invalid");
}

// ============================================================================
//  Non-object parent in path → return default.
// ============================================================================
TEST_F(JsonConfigLoaderTest, PathThroughNonObjectReturnsDefault)
{
    writeFile(R"({"server": 42})");

    JsonConfigLoader cfg(tempPath_);
    ASSERT_TRUE(cfg.load());

    // "server" is an int, not an object, so "server.port" cannot be resolved
    EXPECT_EQ(cfg.getInt("server.port", -1), -1);
}

// ============================================================================
//  getValue returns correct JSON values.
// ============================================================================
TEST_F(JsonConfigLoaderTest, GetValueReturnsJsonValues)
{
    writeFile(R"({
        "array": [1, 2, 3],
        "object": {"key": "val"},
        "number": 3.14
    })");

    JsonConfigLoader cfg(tempPath_);
    ASSERT_TRUE(cfg.load());

    auto arr = cfg.getValue("array");
    ASSERT_TRUE(arr.is_array());
    EXPECT_EQ(arr.size(), 3u);

    auto obj = cfg.getValue("object");
    ASSERT_TRUE(obj.is_object());
    EXPECT_EQ(obj["key"], "val");

    auto num = cfg.getValue("number");
    ASSERT_TRUE(num.is_number());
    EXPECT_DOUBLE_EQ(num.get<double>(), 3.14);
}

// ============================================================================
//  Empty key path → returns root.
// ============================================================================
TEST_F(JsonConfigLoaderTest, EmptyKeyPathReturnsRoot)
{
    writeFile(R"({"key": "val"})");

    JsonConfigLoader cfg(tempPath_);
    ASSERT_TRUE(cfg.load());

    auto root = cfg.getValue("");
    ASSERT_TRUE(root.is_object());
    EXPECT_EQ(root["key"], "val");
}

// ============================================================================
//  Configurable path — constructor accepts any path.
// ============================================================================
TEST_F(JsonConfigLoaderTest, ConfigurablePath)
{
    writeFile(R"({"custom": true})");

    JsonConfigLoader cfg(tempPath_);
    ASSERT_TRUE(cfg.load());

    EXPECT_TRUE(cfg.getBool("custom"));
}
