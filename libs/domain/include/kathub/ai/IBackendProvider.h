#pragma once

#include <string>

namespace kathub {
namespace ai {

/// Abstract AI backend provider interface.
///
/// Implementations wrap specific AI backends (OpenAI, Ollama, local models)
/// behind a common API so the rest of the system does not depend on any
/// particular provider.
class IBackendProvider
{
public:
    virtual ~IBackendProvider() = default;

    /// One-time setup with provider-specific configuration.
    /// @param config  JSON string or path to a config file.
    /// @return true on success.
    virtual bool initialize(const std::string &config) = 0;

    /// Send a chat prompt and wait for the full response.
    /// @param prompt  User message or conversation context.
    /// @return The model's response as a string.
    virtual std::string chat(const std::string &prompt) = 0;

    /// Return a human-readable description of what this backend can do.
    /// Examples: "text-completion", "chat,streaming", "vision,chat".
    virtual std::string getCapabilities() = 0;

    /// Return the backend's display name.
    /// Examples: "OpenAI", "Ollama", "Local-LLM".
    virtual std::string getName() = 0;
};

} // namespace ai
} // namespace kathub
