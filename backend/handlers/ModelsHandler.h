#pragma once

#include "IHttpHandler.h"

#include <string>

class ModelsHandler : public IHttpHandler
{
public:
    ModelsHandler();
    const char* route() override;
    HttpMethod  method() override;
    void        handle(const char* request, void* response) override;
    void        handleWithContext(const char* body, const char* path,
                                  const char* query, void* response,
                                  const char* method = nullptr) override;
};
