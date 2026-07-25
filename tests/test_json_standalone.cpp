#include <gtest/gtest.h>

#include "kathtech/config/JsonConfigLoader.h"

#include <fstream>
#include <cstdio>

using namespace KatHub;

class JsonConfigLoaderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tempPath_ = "test_kathub_config.json";
        std::remove(tempPath_.c_str());
    }

    void TearDown() override
    {
        std::remove(tempPath_.c_str());
    }

    void writeFile(const std::string &content)
    {
        std::ofstream f(tempPath_);
        ASSERT_TRUE(f.is_open());
        f << content;
        f.close();
    }

    std::string tempPath_;
};

TEST_F(JsonConfigLoaderTest, MissingFileReturnsDefaults)
{
    JsonConfigLoader loader("__nonexistent_file__.json");
    EXPECT_FALSE(loader.load());
    EXPECT_FALSE(loader.loaded());
    EXPECT_FALSE(loader.lastError().empty());

    EXPECT_EQ(loader.getString("key", "fallback"), "fallback");
    EXPECT_EQ(loader.getInt("port", 8080), 8080);
    EXPECT_EQ(loader.getBool("debug", true), true);
}

TEST_F(JsonConfigLoaderTest, EmptyJsonSucceeds)
{
    writeFile("{}");
    JsonConfigLoader loader(tempPath_);
    ASSERT_TRUE(loader.load());
    EXPECT_TRUE(loader.loaded());
    EXPECT_TRUE(loader.lastError().empty());
}

TEST_F(JsonConfigLoaderTest, SimpleKeyRead)
{
    writeFile(R"({"name":"KatHub","port":4242,"debug":true})");
    JsonConfigLoader loader(tempPath_);
    ASSERT_TRUE(loader.load());

    EXPECT_EQ(loader.getString("name"), "KatHub");
    EXPECT_EQ(loader.getInt("port"), 4242);
    EXPECT_EQ(loader.getBool("debug"), true);
}

TEST_F(JsonConfigLoaderTest, NestedKeyPath)
{
    writeFile(R"({"server":{"host":"0.0.0.0","port":8080}})");
    JsonConfigLoader loader(tempPath_);
    ASSERT_TRUE(loader.load());

    EXPECT_EQ(loader.getString("server.host"), "0.0.0.0");
    EXPECT_EQ(loader.getInt("server.port"), 8080);
}

TEST_F(JsonConfigLoaderTest, MissingNestedKeyReturnsDefault)
{
    writeFile(R"({"server":{"port":8080}})");
    JsonConfigLoader loader(tempPath_);
    ASSERT_TRUE(loader.load());

    EXPECT_EQ(loader.getString("server.host", "127.0.0.1"), "127.0.0.1");
    EXPECT_EQ(loader.getInt("server.timeout", 30), 30);
}

TEST_F(JsonConfigLoaderTest, ConfigurablePath)
{
    writeFile(R"({"key":"value"})");
    JsonConfigLoader loader(tempPath_);
    ASSERT_TRUE(loader.load());

    EXPECT_EQ(loader.getString("key"), "value");
}

TEST_F(JsonConfigLoaderTest, MalformedJsonUsesDefaults)
{
    writeFile("{not valid json");
    JsonConfigLoader loader(tempPath_);
    EXPECT_FALSE(loader.load());
    EXPECT_FALSE(loader.loaded());

    EXPECT_EQ(loader.getString("key", "safe"), "safe");
    EXPECT_EQ(loader.getInt("num", 99), 99);
}

TEST_F(JsonConfigLoaderTest, GetValueReturnsJsonValues)
{
    writeFile(R"({"arr":[1,2,3],"obj":{"a":1},"num":42})");
    JsonConfigLoader loader(tempPath_);
    ASSERT_TRUE(loader.load());

    EXPECT_EQ(loader.getInt("num"), 42);
    EXPECT_TRUE(loader.getValue("arr").is_array());
    EXPECT_TRUE(loader.getValue("obj").is_object());
}

TEST_F(JsonConfigLoaderTest, EmptyKeyPathReturnsRoot)
{
    writeFile(R"({"root":"value"})");
    JsonConfigLoader loader(tempPath_);
    ASSERT_TRUE(loader.load());

    auto root = loader.getValue("");
    EXPECT_TRUE(root.is_object());
    EXPECT_EQ(root["root"], "value");
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
