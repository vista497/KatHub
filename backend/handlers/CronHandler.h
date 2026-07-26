#pragma once

#include "IHttpHandler.h"

#include <string>

class CronHandler : public IHttpHandler
{
public:
    CronHandler();
    const char* route() override;
    HttpMethod  method() override;
    void        handle(const char* request, void* response) override;
    void        handleWithContext(const char* body, const char* path,
                                  const char* query, void* response,
                                  const char* method = nullptr) override;
};
