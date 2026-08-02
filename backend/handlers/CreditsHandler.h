#pragma once

#include <string>

#include "IHttpHandler.h"

// GET /api/credits → {"credits": <number>}
// Reads RouterAI API key from .env (HERMES_CUSTOM_ROUTERAI_RU_API_KEY) and
// queries https://routerai.ru/api/v1/credits to get the current balance.
class CreditsHandler : public IHttpHandler
{
public:
    CreditsHandler() = default;

    const char* route() override;
    HttpMethod method() override;
    void handle(const char* body, void* response) override;

    // Reads the RouterAI secret token from the same .env files KatHub uses.
    static std::string loadRouterAiKey();
};
