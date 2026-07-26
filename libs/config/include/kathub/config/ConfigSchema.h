#pragma once

#include <nlohmann/json.hpp>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace kathub::config {

// ============================================================================
// FieldType — supported JSON field types
// ============================================================================

enum class FieldType
{
    String,
    Int,
    Bool,
    Array,
    Object
};

// ============================================================================
// ConfigSchema — forward declaration for self-referential types
// ============================================================================

class ConfigSchema;

// ============================================================================
// FieldSchema — describes one field in a ConfigSchema
// ============================================================================

class FieldSchema
{
public:
    std::string name;
    FieldType type = FieldType::String;
    bool required = false;
    std::optional<nlohmann::json> defaultValue;

    // For Array fields: item type (if primitive) or schema (if objects)
    std::optional<FieldType> arrayItemType;
    std::unique_ptr<ConfigSchema> arrayItemSchema;

    // For Object fields: nested schema
    std::unique_ptr<ConfigSchema> nestedSchema;

    FieldSchema() = default;

    explicit FieldSchema(std::string fieldName, FieldType fieldType)
        : name(std::move(fieldName)), type(fieldType)
    {}
};

// ============================================================================
// ConfigSchema — schema for a JSON object
// ============================================================================

class ConfigSchema
{
public:
    // ------------------------------------------------------------------------
    // Construction helpers (builder pattern)
    // ------------------------------------------------------------------------

    /// Add a field and return a reference to its FieldSchema for chaining.
    FieldSchema &addField(std::string name, FieldType type)
    {
        auto [it, inserted] = fields_.try_emplace(
            name, std::make_unique<FieldSchema>(name, type));
        fieldOrder_.push_back(name);
        return *it->second;
    }

    /// Shortcut: add a required field.
    FieldSchema &addRequired(std::string name, FieldType type)
    {
        auto &f = addField(std::move(name), type);
        f.required = true;
        return f;
    }

    /// Shortcut: add an optional field with a default value.
    FieldSchema &addOptional(std::string name, FieldType type,
                             nlohmann::json defaultValue)
    {
        auto &f = addField(std::move(name), type);
        f.defaultValue = std::move(defaultValue);
        return f;
    }

    /// Look up a field by name; returns nullptr if not found.
    const FieldSchema *findField(const std::string &name) const
    {
        auto it = fields_.find(name);
        return (it != fields_.end()) ? it->second.get() : nullptr;
    }

    FieldSchema *findField(const std::string &name)
    {
        auto it = fields_.find(name);
        return (it != fields_.end()) ? it->second.get() : nullptr;
    }

    /// Access all field names in insertion order.
    const std::vector<std::string> &fieldNames() const { return fieldOrder_; }

    // ------------------------------------------------------------------------
    // Validation
    // ------------------------------------------------------------------------

    /// Validate a JSON object against this schema.
    /// @return A list of error messages. Empty list = valid.
    std::vector<std::string> validate(const nlohmann::json &data) const
    {
        std::vector<std::string> errors;

        if (!data.is_object())
        {
            errors.push_back("Root value must be a JSON object");
            return errors;
        }

        for (const auto &name : fieldOrder_)
        {
            const auto &field = *fields_.at(name);
            auto it = data.find(name);

            if (it == data.end())
            {
                // Field missing
                if (field.required)
                {
                    errors.push_back(
                        "Missing required field: '" + name + "'");
                }
                // Optional — ok, default will be applied later
                continue;
            }

            // Validate type
            validateFieldType(name, *it, field, errors);
        }

        // Check for unknown fields (strict mode — warn about them)
        for (auto it = data.begin(); it != data.end(); ++it)
        {
            if (fields_.find(it.key()) == fields_.end())
            {
                errors.push_back("Unknown field: '" + it.key() + "'");
            }
        }

        return errors;
    }

    // ------------------------------------------------------------------------
    // Defaults
    // ------------------------------------------------------------------------

    /// Apply default values for missing optional fields.
    /// Mutates the JSON in place.
    void applyDefaults(nlohmann::json &data) const
    {
        if (!data.is_object())
        {
            data = nlohmann::json::object();
        }

        for (const auto &name : fieldOrder_)
        {
            const auto &field = *fields_.at(name);

            if (data.find(name) == data.end() && field.defaultValue.has_value())
            {
                data[name] = field.defaultValue.value();
            }
        }
    }

    // ------------------------------------------------------------------------
    // Combined process (validate + defaults)
    // ------------------------------------------------------------------------

    struct Result
    {
        bool valid = false;
        nlohmann::json data;
        std::vector<std::string> errors;
    };

    /// Validate and apply defaults in one pass.
    /// On success, `result.data` contains the config with defaults applied.
    Result process(const nlohmann::json &input) const
    {
        Result result;
        result.errors = validate(input);
        result.valid = result.errors.empty();

        if (result.valid)
        {
            result.data = input;
            applyDefaults(result.data);
        }

        return result;
    }

    // ------------------------------------------------------------------------
    // Factory: build a ConfigSchema from a JSON schema definition
    // ------------------------------------------------------------------------
    //
    // Schema definition format:
    // {
    //     "fieldName": {
    //         "type": "string" | "int" | "bool" | "array" | "object",
    //         "required": true | false,          // default: false
    //         "default": <any>,                   // optional default value
    //         "items": { "type": "string" } | {   // for arrays: item schema def
    //             "type": "object",
    //             "fields": { ... }
    //         },
    //         "fields": { ... }                   // for objects: nested fields
    //     }
    // }
    // ------------------------------------------------------------------------

    static ConfigSchema fromJson(const nlohmann::json &schemaDef)
    {
        ConfigSchema schema;

        if (!schemaDef.is_object())
        {
            return schema; // empty schema for non-object
        }

        for (auto it = schemaDef.begin(); it != schemaDef.end(); ++it)
        {
            const std::string &name = it.key();
            const auto &def = it.value();

            if (!def.is_object())
            {
                continue; // skip invalid field definitions
            }

            // Parse type
            FieldType ft = parseFieldType(def.value("type", "string"));

            // Create field
            auto &field = schema.addField(name, ft);

            // Required
            if (def.contains("required") && def["required"].is_boolean())
            {
                field.required = def["required"].get<bool>();
            }

            // Default value
            if (def.contains("default"))
            {
                field.defaultValue = def["default"];
            }

            // Items (for arrays)
            if (def.contains("items"))
            {
                const auto &itemsDef = def["items"];
                if (itemsDef.is_object())
                {
                    std::string itemType =
                        itemsDef.value("type", "string");
                    FieldType itm = parseFieldType(itemType);

                    if (itm == FieldType::Object && itemsDef.contains("fields"))
                    {
                        field.arrayItemSchema = std::make_unique<ConfigSchema>(
                            fromJson(itemsDef["fields"]));
                    }
                    else
                    {
                        field.arrayItemType = itm;
                    }
                }
            }

            // Fields (for nested objects)
            if (def.contains("fields") && def["fields"].is_object())
            {
                field.nestedSchema = std::make_unique<ConfigSchema>(
                    fromJson(def["fields"]));
            }
        }

        return schema;
    }

    // ------------------------------------------------------------------------
    // JSON serialization (schema → JSON)
    // ------------------------------------------------------------------------

    nlohmann::json toJson() const
    {
        nlohmann::json result = nlohmann::json::object();

        for (const auto &name : fieldOrder_)
        {
            const auto &field = *fields_.at(name);
            nlohmann::json def = nlohmann::json::object();

            def["type"] = fieldTypeToString(field.type);
            def["required"] = field.required;

            if (field.defaultValue.has_value())
            {
                def["default"] = field.defaultValue.value();
            }

            if (field.arrayItemType.has_value())
            {
                def["items"] = nlohmann::json::object();
                def["items"]["type"] =
                    fieldTypeToString(field.arrayItemType.value());
            }
            else if (field.arrayItemSchema)
            {
                def["items"] = nlohmann::json::object();
                def["items"]["type"] = "object";
                def["items"]["fields"] = field.arrayItemSchema->toJson();
            }

            if (field.nestedSchema)
            {
                def["type"] = "object";
                def["fields"] = field.nestedSchema->toJson();
            }

            result[name] = std::move(def);
        }

        return result;
    }

    // ------------------------------------------------------------------------
    // Comparison
    // ------------------------------------------------------------------------

    bool empty() const { return fields_.empty(); }
    size_t size() const { return fields_.size(); }

private:
    // ------------------------------------------------------------------------
    // Internal helpers
    // ------------------------------------------------------------------------

    static FieldType parseFieldType(const std::string &s)
    {
        if (s == "int")
            return FieldType::Int;
        if (s == "bool" || s == "boolean")
            return FieldType::Bool;
        if (s == "array")
            return FieldType::Array;
        if (s == "object")
            return FieldType::Object;
        return FieldType::String; // default
    }

    static const char *fieldTypeToString(FieldType t)
    {
        switch (t)
        {
        case FieldType::String:
            return "string";
        case FieldType::Int:
            return "int";
        case FieldType::Bool:
            return "bool";
        case FieldType::Array:
            return "array";
        case FieldType::Object:
            return "object";
        }
        return "string";
    }

    void validateFieldType(const std::string &name,
                           const nlohmann::json &value,
                           const FieldSchema &field,
                           std::vector<std::string> &errors) const
    {
        switch (field.type)
        {
        case FieldType::String:
            if (!value.is_string())
            {
                errors.push_back("Field '" + name +
                                 "': expected string, got " +
                                 std::string(value.type_name()));
            }
            break;

        case FieldType::Int:
            if (!value.is_number_integer())
            {
                errors.push_back("Field '" + name +
                                 "': expected integer, got " +
                                 std::string(value.type_name()));
            }
            break;

        case FieldType::Bool:
            if (!value.is_boolean())
            {
                errors.push_back("Field '" + name +
                                 "': expected boolean, got " +
                                 std::string(value.type_name()));
            }
            break;

        case FieldType::Array:
            if (!value.is_array())
            {
                errors.push_back("Field '" + name +
                                 "': expected array, got " +
                                 std::string(value.type_name()));
            }
            else if (field.arrayItemType.has_value())
            {
                // Validate each item's type
                for (size_t i = 0; i < value.size(); ++i)
                {
                    validatePrimitiveType(
                        name + "[" + std::to_string(i) + "]",
                        value[i], field.arrayItemType.value(), errors);
                }
            }
            else if (field.arrayItemSchema)
            {
                for (size_t i = 0; i < value.size(); ++i)
                {
                    auto itemErrors =
                        field.arrayItemSchema->validate(value[i]);
                    for (auto &e : itemErrors)
                    {
                        errors.push_back(name + "[" +
                                         std::to_string(i) + "]: " +
                                         e);
                    }
                }
            }
            break;

        case FieldType::Object:
            if (!value.is_object())
            {
                errors.push_back("Field '" + name +
                                 "': expected object, got " +
                                 std::string(value.type_name()));
            }
            else if (field.nestedSchema)
            {
                auto nestedErrors = field.nestedSchema->validate(value);
                for (auto &e : nestedErrors)
                {
                    errors.push_back(name + "." + e);
                }
            }
            break;
        }
    }

    static void validatePrimitiveType(const std::string &path,
                                      const nlohmann::json &value,
                                      FieldType expected,
                                      std::vector<std::string> &errors)
    {
        switch (expected)
        {
        case FieldType::String:
            if (!value.is_string())
                errors.push_back(path + ": expected string, got " +
                                 std::string(value.type_name()));
            break;
        case FieldType::Int:
            if (!value.is_number_integer())
                errors.push_back(path + ": expected integer, got " +
                                 std::string(value.type_name()));
            break;
        case FieldType::Bool:
            if (!value.is_boolean())
                errors.push_back(path + ": expected boolean, got " +
                                 std::string(value.type_name()));
            break;
        default:
            break;
        }
    }

    std::unordered_map<std::string, std::unique_ptr<FieldSchema>> fields_;
    std::vector<std::string> fieldOrder_;
};

} // namespace kathub::config
