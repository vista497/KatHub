#include "kathtech/config/JsonConfigLoader.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace KatHub {

// ============================================================================
//  Construction
// ============================================================================

JsonConfigLoader::JsonConfigLoader(const std::string &path)
    : path_(path)
    , data_(nlohmann::json::object())
{
}

// ============================================================================
//  load()
// ============================================================================

bool JsonConfigLoader::load()
{
    data_      = nlohmann::json::object();
    loaded_    = false;
    lastError_.clear();

    std::ifstream file(path_);
    if (!file.is_open())
    {
        lastError_ = "Cannot open config file: " + path_;
        std::cerr << "JsonConfigLoader: " << lastError_ << std::endl;
        return false;
    }

    try
    {
        file >> data_;
    }
    catch (const nlohmann::json::parse_error &e)
    {
        lastError_ = std::string("JSON parse error: ") + e.what();
        std::cerr << "JsonConfigLoader: " << lastError_ << std::endl;
        data_ = nlohmann::json::object();
        return false;
    }

    if (!data_.is_object())
    {
        lastError_ = "Config root must be a JSON object";
        std::cerr << "JsonConfigLoader: " << lastError_ << std::endl;
        data_ = nlohmann::json::object();
        return false;
    }

    loaded_ = true;
    return true;
}

// ============================================================================
//  applyEnvOverrides()
// ============================================================================

int JsonConfigLoader::applyEnvOverrides()
{
    int applied = 0;

#ifdef _MSC_VER
    char **envp = _environ;
#else
    extern char **environ;
    char **envp = environ;
#endif

    for (char **env = envp; *env != nullptr; ++env)
    {
        std::string entry(*env);
        auto eq = entry.find('=');
        if (eq == std::string::npos)
            continue;

        std::string name  = entry.substr(0, eq);
        std::string value = entry.substr(eq + 1);

        if (name.rfind("KATHUB_", 0) != 0)
            continue;

        std::string configKey = envToKey(name);
        if (configKey.empty())
            continue;

        nlohmann::json parsed = parseEnvValue(value);
        setPath(configKey, parsed);
        ++applied;
    }

    return applied;
}

// ============================================================================
//  envToKey()
// ============================================================================

std::string JsonConfigLoader::envToKey(const std::string &envName)
{
    // Strip "KATHUB_" prefix (7 chars)
    std::string key = envName.substr(7);

    std::string result;
    result.reserve(key.size());
    for (char c : key)
    {
        if (c == '_')
            result += '.';
        else
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

// ============================================================================
//  parseEnvValue()
// ============================================================================

nlohmann::json JsonConfigLoader::parseEnvValue(const std::string &rawValue)
{
    // Try integer
    {
        bool isInt = !rawValue.empty();
        for (size_t i = 0; i < rawValue.size(); ++i)
        {
            if (i == 0 && rawValue[i] == '-')
                continue;
            if (!std::isdigit(static_cast<unsigned char>(rawValue[i])))
            {
                isInt = false;
                break;
            }
        }
        if (isInt)
        {
            try { return nlohmann::json(std::stoll(rawValue)); }
            catch (...) { /* fall through */ }
        }
    }

    // Try boolean
    {
        std::string lower = rawValue;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower == "true" || lower == "1")
            return nlohmann::json(true);
        if (lower == "false" || lower == "0")
            return nlohmann::json(false);
    }

    // Try JSON (arrays/objects)
    if (!rawValue.empty() && (rawValue[0] == '{' || rawValue[0] == '['))
    {
        try { return nlohmann::json::parse(rawValue); }
        catch (...) { /* fall through */ }
    }

    // Default: string
    return nlohmann::json(rawValue);
}

// ============================================================================
//  setPath()
// ============================================================================

void JsonConfigLoader::setPath(const std::string &keyPath, const nlohmann::json &value)
{
    if (keyPath.empty())
    {
        if (value.is_object())
            data_ = value;
        return;
    }

    std::vector<std::string> parts;
    std::istringstream stream(keyPath);
    std::string segment;
    while (std::getline(stream, segment, '.'))
    {
        if (!segment.empty())
            parts.push_back(segment);
    }

    if (parts.empty())
        return;

    nlohmann::json *current = &data_;
    for (size_t i = 0; i < parts.size() - 1; ++i)
    {
        if (!current->is_object())
            *current = nlohmann::json::object();
        if (!current->contains(parts[i]) || !(*current)[parts[i]].is_object())
            (*current)[parts[i]] = nlohmann::json::object();
        current = &(*current)[parts[i]];
    }

    (*current)[parts.back()] = value;
}

// ============================================================================
//  resolvePath()
// ============================================================================

nlohmann::json JsonConfigLoader::resolvePath(const std::string &keyPath) const
{
    if (keyPath.empty())
        return data_;

    const nlohmann::json *current = &data_;

    std::string::size_type start = 0;
    while (start <= keyPath.size())
    {
        auto dot = keyPath.find('.', start);
        std::string segment = (dot == std::string::npos)
            ? keyPath.substr(start)
            : keyPath.substr(start, dot - start);

        if (!current->is_object() || !current->contains(segment))
            return nlohmann::json();

        current = &(*current)[segment];

        if (dot == std::string::npos)
            break;
        start = dot + 1;
    }

    return *current;
}

// ============================================================================
//  getString()
// ============================================================================

std::string JsonConfigLoader::getString(const std::string &keyPath,
                                         const std::string &defaultValue) const
{
    auto val = resolvePath(keyPath);
    if (val.is_null())
        return defaultValue;
    if (val.is_string())
        return val.get<std::string>();
    return defaultValue;
}

// ============================================================================
//  getInt()
// ============================================================================

int JsonConfigLoader::getInt(const std::string &keyPath, int defaultValue) const
{
    auto val = resolvePath(keyPath);
    if (val.is_null())
        return defaultValue;
    if (val.is_number_integer())
        return val.get<int>();
    return defaultValue;
}

// ============================================================================
//  getBool()
// ============================================================================

bool JsonConfigLoader::getBool(const std::string &keyPath, bool defaultValue) const
{
    auto val = resolvePath(keyPath);
    if (val.is_null())
        return defaultValue;
    if (val.is_boolean())
        return val.get<bool>();
    return defaultValue;
}

// ============================================================================
//  getValue()
// ============================================================================

nlohmann::json JsonConfigLoader::getValue(const std::string &keyPath,
                                           const nlohmann::json &defaultValue) const
{
    auto val = resolvePath(keyPath);
    if (!val.is_null())
        return val;
    return defaultValue;
}

} // namespace KatHub
