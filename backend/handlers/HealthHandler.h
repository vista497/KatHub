#pragma once

#include "IHttpHandler.h"

#include <string>

// HealthHandler — local machine health metrics (CPU / RAM / disk / uptime).
// GET /api/health → JSON: { cpu_usage, memory_total, memory_used,
//                           memory_percent, disk_total, disk_used,
//                           disk_percent, uptime }
// Uses Win32 APIs directly (no external deps).

class HealthHandler : public IHttpHandler
{
public:
    HealthHandler();

    const char* route() override;
    HttpMethod  method() override;
    void        handle(const char* request, void* response) override;
};
