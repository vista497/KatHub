#include <gtest/gtest.h>

#include "ConfigService.h"

#include <QCoreApplication>
#include <QFile>
#include <QProcessEnvironment>

#include <cstdio>  // std::remove
#include <fstream>
#include <string>

// ============================================================================
//  Fixture — manages a temporary config file per test.
// ============================================================================
class ConfigServiceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tempPath_ = "test_config_service.json";
        std::remove(tempPath_.c_str());

        // Clean any leftover env vars from previous tests.
        QProcessEnvironment sysEnv = QProcessEnvironment::systemEnvironment();
        for (const auto &v : envVarsSet_) {
            qunsetenv(v.c_str());
        }
        envVarsSet_.clear();
    }

    void TearDown() override
    {
        std::remove(tempPath_.c_str());
        for (const auto &v : envVarsSet_) {
            qunsetenv(v.c_str());
        }
        envVarsSet_.clear();
    }

    void writeFile(const std::string &content)
    {
        std::ofstream f(tempPath_);
        if (!f.is_open())
            throw std::runtime_error("Failed to create temp config file");
        f << content;
        f.close();
    }

    void setEnv(const std::string &name, const std::string &value)
    {
        qputenv(name.c_str(), QByteArray::fromStdString(value));
        envVarsSet_.insert(name);
    }

    std::string tempPath_;
    std::set<std::string> envVarsSet_;
};

// ============================================================================
//  1. Valid config loads
// ============================================================================

TEST_F(ConfigServiceTest, LoadValidJsonSucceeds)
{
    writeFile(R"({"port": 8080, "host": "localhost"})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));
    EXPECT_EQ(svc.filePath(), tempPath_);
    EXPECT_TRUE(svc.lastError().empty());
}

TEST_F(ConfigServiceTest, LoadMissingFileFails)
{
    ConfigService svc;
    ASSERT_FALSE(svc.loadFromFile("__nonexistent_file__.json"));
    EXPECT_FALSE(svc.lastError().empty());
    EXPECT_TRUE(svc.lastError().find("Cannot open") != std::string::npos);
}

TEST_F(ConfigServiceTest, LoadMalformedJsonFails)
{
    writeFile("{not valid json at all!!");

    ConfigService svc;
    ASSERT_FALSE(svc.loadFromFile(tempPath_));
    EXPECT_FALSE(svc.lastError().empty());
    EXPECT_TRUE(svc.lastError().find("JSON parse error") != std::string::npos);
}

TEST_F(ConfigServiceTest, LoadNonObjectRootFails)
{
    writeFile("[1, 2, 3]");

    ConfigService svc;
    ASSERT_FALSE(svc.loadFromFile(tempPath_));
    EXPECT_EQ(svc.lastError(), "Config root must be a JSON object");
}

TEST_F(ConfigServiceTest, LoadEmptyObjectSucceeds)
{
    writeFile("{}");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));
    EXPECT_TRUE(svc.lastError().empty());
}

// ============================================================================
//  2. Value access — get / get-with-default / has
// ============================================================================

TEST_F(ConfigServiceTest, GetSimpleValues)
{
    writeFile(R"({
        "port": 9090,
        "host": "0.0.0.0",
        "debug": true,
        "ratio": 3.14
    })");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    auto port = svc.get("port");
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(port->toInt(), 9090);

    auto host = svc.get("host");
    ASSERT_TRUE(host.has_value());
    EXPECT_EQ(host->toString().toStdString(), "0.0.0.0");

    auto debug = svc.get("debug");
    ASSERT_TRUE(debug.has_value());
    EXPECT_EQ(debug->toBool(), true);

    auto ratio = svc.get("ratio");
    ASSERT_TRUE(ratio.has_value());
    EXPECT_DOUBLE_EQ(ratio->toDouble(), 3.14);
}

TEST_F(ConfigServiceTest, GetNestedPath)
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

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    auto port = svc.get("server.port");
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(port->toInt(), 8080);

    auto host = svc.get("server.host");
    ASSERT_TRUE(host.has_value());
    EXPECT_EQ(host->toString().toStdString(), "127.0.0.1");

    auto level = svc.get("logging.level");
    ASSERT_TRUE(level.has_value());
    EXPECT_EQ(level->toString().toStdString(), "debug");
}

TEST_F(ConfigServiceTest, GetDeepNesting)
{
    writeFile(R"({
        "a": { "b": { "c": { "value": 42 } } }
    })");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    auto val = svc.get("a.b.c.value");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val->toInt(), 42);
}

TEST_F(ConfigServiceTest, GetMissingKeyReturnsNullopt)
{
    writeFile(R"({"server": {"port": 8080}})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    EXPECT_FALSE(svc.get("nonexistent").has_value());
    EXPECT_FALSE(svc.get("server.host").has_value());
    EXPECT_FALSE(svc.get("server.port.host").has_value());
}

TEST_F(ConfigServiceTest, GetWithDefault)
{
    writeFile(R"({"server": {"port": 8080}})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    // Existing key — actual value.
    EXPECT_EQ(svc.get("server.port", QVariant(3000)).toInt(), 8080);

    // Missing key — default.
    EXPECT_EQ(svc.get("server.host", QVariant("default-host")).toString().toStdString(),
              "default-host");
    EXPECT_EQ(svc.get("nonexistent", QVariant(42)).toInt(), 42);
}

TEST_F(ConfigServiceTest, HasReturnsCorrectBoolean)
{
    writeFile(R"({"server": {"port": 8080}})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    EXPECT_TRUE(svc.has("server"));
    EXPECT_TRUE(svc.has("server.port"));
    EXPECT_FALSE(svc.has("server.host"));
    EXPECT_FALSE(svc.has("nonexistent"));
}

TEST_F(ConfigServiceTest, PathThroughNonObjectReturnsNullopt)
{
    writeFile(R"({"server": 42})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    // "server" is an int, not an object — "server.port" can't be resolved.
    EXPECT_FALSE(svc.get("server.port").has_value());
    EXPECT_FALSE(svc.has("server.port"));
}

TEST_F(ConfigServiceTest, EmptyPathOnEmptyConfig)
{
    writeFile("{}");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    // Empty path should return Undefined.
    EXPECT_FALSE(svc.get("").has_value());
}

// ============================================================================
//  3. Schema validation — rejects bad types
// ============================================================================

TEST_F(ConfigServiceTest, SchemaValidConfigPasses)
{
    writeFile(R"({"port": 8080, "host": "localhost", "debug": true})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "port":  { "type": "integer" },
            "host":  { "type": "string"  },
            "debug": { "type": "boolean" }
        }
    })";

    EXPECT_TRUE(svc.validate(schema));
    EXPECT_TRUE(svc.lastError().empty());
}

TEST_F(ConfigServiceTest, SchemaRejectsWrongStringType)
{
    writeFile(R"({"port": 8080, "host": 42})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "port": { "type": "integer" },
            "host": { "type": "string"  }
        }
    })";

    EXPECT_FALSE(svc.validate(schema));
    EXPECT_TRUE(svc.lastError().find("expected type \"string\"") != std::string::npos);
}

TEST_F(ConfigServiceTest, SchemaRejectsWrongIntegerType)
{
    writeFile(R"({"port": "not-a-number"})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "port": { "type": "integer" }
        }
    })";

    EXPECT_FALSE(svc.validate(schema));
    EXPECT_TRUE(svc.lastError().find("expected type \"integer\"") != std::string::npos);
}

TEST_F(ConfigServiceTest, SchemaRejectsFloatForInteger)
{
    writeFile(R"({"port": 3.14})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "port": { "type": "integer" }
        }
    })";

    EXPECT_FALSE(svc.validate(schema));
    EXPECT_TRUE(svc.lastError().find("expected type \"integer\"") != std::string::npos);
}

TEST_F(ConfigServiceTest, SchemaRejectsWrongBooleanType)
{
    writeFile(R"({"debug": "yes"})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "debug": { "type": "boolean" }
        }
    })";

    EXPECT_FALSE(svc.validate(schema));
    EXPECT_TRUE(svc.lastError().find("expected type \"boolean\"") != std::string::npos);
}

TEST_F(ConfigServiceTest, SchemaRejectsWrongNumberType)
{
    writeFile(R"({"ratio": "3.14"})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "ratio": { "type": "number" }
        }
    })";

    EXPECT_FALSE(svc.validate(schema));
    EXPECT_TRUE(svc.lastError().find("expected type \"number\"") != std::string::npos);
}

TEST_F(ConfigServiceTest, SchemaRejectsWrongObjectType)
{
    writeFile(R"({"data": "not-an-object"})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "data": { "type": "object" }
        }
    })";

    EXPECT_FALSE(svc.validate(schema));
    EXPECT_TRUE(svc.lastError().find("expected type \"object\"") != std::string::npos);
}

TEST_F(ConfigServiceTest, SchemaRejectsWrongArrayType)
{
    writeFile(R"({"items": "not-an-array"})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "items": { "type": "array" }
        }
    })";

    EXPECT_FALSE(svc.validate(schema));
    EXPECT_TRUE(svc.lastError().find("expected type \"array\"") != std::string::npos);
}

// ============================================================================
//  4. Schema validation — required / properties structure
// ============================================================================

TEST_F(ConfigServiceTest, SchemaMissingRequiredPropertyFails)
{
    writeFile(R"({"host": "localhost"})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "host": { "type": "string" },
            "port": { "type": "integer" }
        },
        "required": ["host", "port"]
    })";

    EXPECT_FALSE(svc.validate(schema));
    EXPECT_TRUE(svc.lastError().find("missing required property") != std::string::npos);
}

TEST_F(ConfigServiceTest, SchemaRequiredEntriesMustBeStrings)
{
    writeFile(R"({"host": "localhost", "port": 8080})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "host": { "type": "string" },
            "port": { "type": "integer" }
        },
        "required": [42]
    })";

    EXPECT_FALSE(svc.validate(schema));
    EXPECT_TRUE(svc.lastError().find("\"required\" entries must be strings") != std::string::npos);
}

TEST_F(ConfigServiceTest, SchemaRequiredMustBeArray)
{
    writeFile(R"({"host": "localhost"})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "host": { "type": "string" }
        },
        "required": "not-an-array"
    })";

    EXPECT_FALSE(svc.validate(schema));
    EXPECT_TRUE(svc.lastError().find("\"required\" must be an array") != std::string::npos);
}

TEST_F(ConfigServiceTest, SchemaPropertiesMustBeObject)
{
    writeFile(R"({"host": "localhost"})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": "not-an-object"
    })";

    EXPECT_FALSE(svc.validate(schema));
    EXPECT_TRUE(svc.lastError().find("\"properties\" must be an object") != std::string::npos);
}

TEST_F(ConfigServiceTest, SchemaTypeMustBeString)
{
    writeFile(R"({"port": 8080})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "port": { "type": 42 }
        }
    })";

    EXPECT_FALSE(svc.validate(schema));
    EXPECT_TRUE(svc.lastError().find("\"type\" must be a string") != std::string::npos);
}

TEST_F(ConfigServiceTest, SchemaMalformedJsonFails)
{
    writeFile(R"({"port": 8080})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    EXPECT_FALSE(svc.validate("{invalid schema json"));
    EXPECT_TRUE(svc.lastError().find("Schema JSON parse error") != std::string::npos);
}

TEST_F(ConfigServiceTest, SchemaRootMustBeObject)
{
    writeFile(R"({"port": 8080})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    // Schema itself is a JSON array, not an object.
    EXPECT_FALSE(svc.validate("[1, 2, 3]"));
    EXPECT_EQ(svc.lastError(), "Schema must be a JSON object");
}

TEST_F(ConfigServiceTest, SchemaRootTypeMustBeObject)
{
    writeFile(R"({"port": 8080})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "string"
    })";

    EXPECT_FALSE(svc.validate(schema));
    EXPECT_EQ(svc.lastError(), "Root schema type must be \"object\"");
}

// ============================================================================
//  5. Nested schema validation
// ============================================================================

TEST_F(ConfigServiceTest, NestedSchemaValidPasses)
{
    writeFile(R"({
        "server": {
            "port": 8080,
            "host": "127.0.0.1"
        }
    })");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "server": {
                "type": "object",
                "properties": {
                    "port": { "type": "integer" },
                    "host": { "type": "string"  }
                },
                "required": ["port", "host"]
            }
        },
        "required": ["server"]
    })";

    EXPECT_TRUE(svc.validate(schema));
}

TEST_F(ConfigServiceTest, NestedSchemaMissingRequiredFails)
{
    writeFile(R"({
        "server": {
            "host": "127.0.0.1"
        }
    })");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "server": {
                "type": "object",
                "properties": {
                    "port": { "type": "integer" },
                    "host": { "type": "string"  }
                },
                "required": ["port", "host"]
            }
        }
    })";

    EXPECT_FALSE(svc.validate(schema));
    EXPECT_TRUE(svc.lastError().find("missing required property") != std::string::npos);
}

TEST_F(ConfigServiceTest, NestedSchemaWrongTypeFails)
{
    writeFile(R"({
        "server": {
            "port": "not-a-number",
            "host": "127.0.0.1"
        }
    })");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "server": {
                "type": "object",
                "properties": {
                    "port": { "type": "integer" },
                    "host": { "type": "string"  }
                }
            }
        }
    })";

    EXPECT_FALSE(svc.validate(schema));
    EXPECT_TRUE(svc.lastError().find("expected type \"integer\"") != std::string::npos);
}

TEST_F(ConfigServiceTest, DeepNestedSchemaValidation)
{
    writeFile(R"({
        "app": {
            "db": {
                "host": "localhost",
                "port": 5432
            }
        }
    })");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "app": {
                "type": "object",
                "properties": {
                    "db": {
                        "type": "object",
                        "properties": {
                            "host": { "type": "string"  },
                            "port": { "type": "integer" }
                        },
                        "required": ["host", "port"]
                    }
                },
                "required": ["db"]
            }
        },
        "required": ["app"]
    })";

    EXPECT_TRUE(svc.validate(schema));
}

// ============================================================================
//  6. Missing optional fields use defaults
// ============================================================================

TEST_F(ConfigServiceTest, OptionalFieldsUseDefaults)
{
    writeFile(R"({"host": "localhost"})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    // port is not in config — get with default should return the fallback.
    EXPECT_EQ(svc.get("port", QVariant(8080)).toInt(), 8080);
    EXPECT_FALSE(svc.has("port"));
}

TEST_F(ConfigServiceTest, SchemaAllowsOptionalFields)
{
    // host is present, port is optional (not in required).
    writeFile(R"({"host": "localhost"})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "host": { "type": "string"  },
            "port": { "type": "integer" }
        },
        "required": ["host"]
    })";

    EXPECT_TRUE(svc.validate(schema));
}

TEST_F(ConfigServiceTest, NestedOptionalFieldsUseDefaults)
{
    writeFile(R"({
        "server": {
            "host": "127.0.0.1"
        }
    })");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    // "server.port" is missing — default is used.
    EXPECT_EQ(svc.get("server.port", QVariant(3000)).toInt(), 3000);
    EXPECT_FALSE(svc.has("server.port"));
}

// ============================================================================
//  7. Environment variable overrides
// ============================================================================

TEST_F(ConfigServiceTest, EnvOverrideAppliesWhenSet)
{
    writeFile(R"({"port": 8080})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    setEnv("KATHUB_PORT", "9090");
    svc.setEnvOverride("port", "KATHUB_PORT");

    auto port = svc.get("port");
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(port->toInt(), 9090);
}

TEST_F(ConfigServiceTest, EnvOverrideFallsBackWhenNotSet)
{
    writeFile(R"({"port": 8080})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    svc.setEnvOverride("port", "KATHUB_NOT_SET_VAR");
    // Env var is NOT set → file value should be used.

    auto port = svc.get("port");
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(port->toInt(), 8080);
}

TEST_F(ConfigServiceTest, EnvOverrideOnlyForRegisteredPath)
{
    writeFile(R"({"port": 8080, "host": "file-host"})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    setEnv("KATHUB_PORT", "9090");
    svc.setEnvOverride("port", "KATHUB_PORT");
    // "host" has NO override registered.

    auto port = svc.get("port");
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(port->toInt(), 9090);

    auto host = svc.get("host");
    ASSERT_TRUE(host.has_value());
    EXPECT_EQ(host->toString().toStdString(), "file-host");
}

TEST_F(ConfigServiceTest, EnvOverrideBoolTrueValues)
{
    writeFile(R"({"debug": false})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    svc.setEnvOverride("debug", "KATHUB_DEBUG");

    // Test various truthy values.
    setEnv("KATHUB_DEBUG", "true");
    EXPECT_EQ(svc.get("debug")->toBool(), true);

    setEnv("KATHUB_DEBUG", "1");
    EXPECT_EQ(svc.get("debug")->toBool(), true);

    setEnv("KATHUB_DEBUG", "yes");
    EXPECT_EQ(svc.get("debug")->toBool(), true);

    setEnv("KATHUB_DEBUG", "True");
    EXPECT_EQ(svc.get("debug")->toBool(), true);
}

TEST_F(ConfigServiceTest, EnvOverrideBoolFalseValues)
{
    writeFile(R"({"debug": true})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    svc.setEnvOverride("debug", "KATHUB_DEBUG");

    setEnv("KATHUB_DEBUG", "false");
    EXPECT_EQ(svc.get("debug")->toBool(), false);

    setEnv("KATHUB_DEBUG", "0");
    EXPECT_EQ(svc.get("debug")->toBool(), false);

    setEnv("KATHUB_DEBUG", "no");
    EXPECT_EQ(svc.get("debug")->toBool(), false);

    setEnv("KATHUB_DEBUG", "False");
    EXPECT_EQ(svc.get("debug")->toBool(), false);
}

TEST_F(ConfigServiceTest, EnvOverrideStringValue)
{
    writeFile(R"({"host": "file-host"})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    setEnv("KATHUB_HOST", "env-host");
    svc.setEnvOverride("host", "KATHUB_HOST");

    auto host = svc.get("host");
    ASSERT_TRUE(host.has_value());
    EXPECT_EQ(host->toString().toStdString(), "env-host");
}

TEST_F(ConfigServiceTest, EnvOverrideNestedPath)
{
    writeFile(R"({"server": {"port": 8080}})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    setEnv("KATHUB_SERVER_PORT", "3000");
    svc.setEnvOverride("server.port", "KATHUB_SERVER_PORT");

    auto port = svc.get("server.port");
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(port->toInt(), 3000);
}

TEST_F(ConfigServiceTest, EnvOverrideNumberFallbackToString)
{
    writeFile(R"({"port": 8080})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    setEnv("KATHUB_PORT", "not-a-number");
    svc.setEnvOverride("port", "KATHUB_PORT");

    auto port = svc.get("port");
    ASSERT_TRUE(port.has_value());
    // Cannot parse as int or double → falls back to string.
    EXPECT_EQ(port->toString().toStdString(), "not-a-number");
}

TEST_F(ConfigServiceTest, EnvOverrideBoolUnrecognisedFallsBackToString)
{
    writeFile(R"({"debug": false})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    setEnv("KATHUB_DEBUG", "maybe");
    svc.setEnvOverride("debug", "KATHUB_DEBUG");

    auto debug = svc.get("debug");
    ASSERT_TRUE(debug.has_value());
    // Unrecognised bool string → fallback to string.
    EXPECT_EQ(debug->toString().toStdString(), "maybe");
}

// ============================================================================
//  8. Config reload
// ============================================================================

TEST_F(ConfigServiceTest, ReloadAfterLoadSucceeds)
{
    writeFile(R"({"version": 1})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));
    EXPECT_EQ(svc.get("version")->toInt(), 1);

    // Change the file on disk.
    writeFile(R"({"version": 2})");

    ASSERT_TRUE(svc.reload());
    EXPECT_EQ(svc.get("version")->toInt(), 2);
}

TEST_F(ConfigServiceTest, ReloadWithoutPriorLoadFails)
{
    ConfigService svc;
    ASSERT_FALSE(svc.reload());
    EXPECT_EQ(svc.lastError(), "No file loaded yet — nothing to reload");
}

TEST_F(ConfigServiceTest, ReloadPicksUpNewKeys)
{
    writeFile(R"({"version": 1})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));
    EXPECT_FALSE(svc.has("newKey"));

    // Add a new key.
    writeFile(R"({"version": 1, "newKey": "hello"})");

    ASSERT_TRUE(svc.reload());
    EXPECT_TRUE(svc.has("newKey"));
    EXPECT_EQ(svc.get("newKey")->toString().toStdString(), "hello");
}

TEST_F(ConfigServiceTest, ReloadRemovesOldKeys)
{
    writeFile(R"({"version": 1, "oldKey": "value"})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));
    EXPECT_TRUE(svc.has("oldKey"));

    // Remove oldKey from file.
    writeFile(R"({"version": 1})");

    ASSERT_TRUE(svc.reload());
    EXPECT_FALSE(svc.has("oldKey"));
}

// ============================================================================
//  9. Edge cases & misc
// ============================================================================

TEST_F(ConfigServiceTest, RawConfigReturnsLoadedObject)
{
    writeFile(R"({"port": 8080, "host": "localhost"})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const QJsonObject &raw = svc.rawConfig();
    EXPECT_EQ(raw.value("port").toInt(), 8080);
    EXPECT_EQ(raw.value("host").toString().toStdString(), "localhost");
}

TEST_F(ConfigServiceTest, LastErrorClearedOnSuccess)
{
    writeFile(R"({"port": 8080})");

    ConfigService svc;

    // First: trigger an error.
    svc.loadFromFile("__nonexistent__");
    EXPECT_FALSE(svc.lastError().empty());

    // Then: successful load should clear the error.
    ASSERT_TRUE(svc.loadFromFile(tempPath_));
    EXPECT_TRUE(svc.lastError().empty());
}

TEST_F(ConfigServiceTest, LoadTwiceReplacesConfig)
{
    // First config.
    writeFile(R"({"version": 1, "host": "first"})");
    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));
    EXPECT_EQ(svc.get("version")->toInt(), 1);
    EXPECT_EQ(svc.get("host")->toString().toStdString(), "first");

    // Second config — different filename simulation (reuse same file with different content).
    writeFile(R"({"version": 2, "port": 3000})");
    ASSERT_TRUE(svc.loadFromFile(tempPath_));
    EXPECT_EQ(svc.get("version")->toInt(), 2);
    EXPECT_FALSE(svc.has("host"));    // old key gone
    EXPECT_TRUE(svc.has("port"));     // new key present
}

TEST_F(ConfigServiceTest, ValidateWithoutLoad)
{
    ConfigService svc;

    const std::string schema = R"({
        "type": "object",
        "properties": {}
    })";

    // Should still work — validates empty config (default constructed).
    EXPECT_TRUE(svc.validate(schema));
}

TEST_F(ConfigServiceTest, SchemaIntegerAcceptsZero)
{
    writeFile(R"({"port": 0})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "port": { "type": "integer" }
        }
    })";

    EXPECT_TRUE(svc.validate(schema));
}

TEST_F(ConfigServiceTest, SchemaIntegerAcceptsNegative)
{
    writeFile(R"({"offset": -42})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "offset": { "type": "integer" }
        }
    })";

    EXPECT_TRUE(svc.validate(schema));
}

TEST_F(ConfigServiceTest, SchemaIntegerAcceptsZeroFractional)
{
    // 42.0 is technically a double but its fractional part is 0, so it's integer.
    writeFile(R"({"port": 42.0})");

    ConfigService svc;
    ASSERT_TRUE(svc.loadFromFile(tempPath_));

    const std::string schema = R"({
        "type": "object",
        "properties": {
            "port": { "type": "integer" }
        }
    })";

    EXPECT_TRUE(svc.validate(schema));
}
