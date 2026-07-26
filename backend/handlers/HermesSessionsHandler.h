#pragma once

#include "IHttpHandler.h"
#include "HermesApiClient.h"

#include <memory>
#include <string>

class HermesSessionsHandler : public IHttpHandler
{
public:
    HermesSessionsHandler();
    const char* route() override;
    HttpMethod method() override;
    void handle(const char* request, void* response) override;
    void handleWithContext(const char* body, const char* path,
                          const char* query, void* response,
                          const char* method = nullptr) override;
    void setApiClient(std::shared_ptr<HermesApiClient> client);

private:
    std::shared_ptr<HermesApiClient> api_;
};
