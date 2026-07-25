#pragma once

#include <kathub/ai/IBackendProvider.h>

#include <string>
#include <memory>

namespace httplib {
class Client;
}

namespace KatHub {

/// OpenRouter API client using cpp-httplib.
///
/// Implements IBackendProvider so it can be used with AIService
/// and instantiated by BackendLoader.
///
/// Config JSON format:
///   {
///     "endpoint": "https://openrouter.ai/api/v1",
///     "api_key": "sk-or-v1-...",
///     "model": "openai/gpt-4o",
///     "parameters": { "temperature": 0.7, "max_tokens": 4096 }
///   }
class OpenRouterClient : public kathub::ai::IBackendProvider
{
public:
    OpenRouterClient();
    ~OpenRouterClient() override;

    // IBackendProvider interface
    bool initialize(const std::string &config) override;
    std::string chat(const std::string &prompt) override;
    std::string getCapabilities() override;
    std::string getName() override;

private:
    std::string m_endpoint;
    std::string m_apiKey;
    std::string m_model;
    std::string m_name;
    double      m_temperature = 0.7;
    int         m_maxTokens   = 4096;
    std::unique_ptr<httplib::Client> m_client;
    bool        m_initialized = false;
};

} // namespace KatHub
