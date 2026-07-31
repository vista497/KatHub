#include "HealthHandler.h"

#include "httplib.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <sstream>
#include <string>

HealthHandler::HealthHandler() = default;

const char* HealthHandler::route()
{
    return "/api/health";
}

IHttpHandler::HttpMethod HealthHandler::method()
{
    return HttpMethod::GET;
}

// ---------------------------------------------------------------------------
// CPU usage: GetSystemTimes delta between two samples (100ms apart).
// ---------------------------------------------------------------------------
static double cpuUsagePercent()
{
#ifdef _WIN32
    FILETIME idle1, kern1, user1;
    if (!GetSystemTimes(&idle1, &kern1, &user1)) return 0.0;

    Sleep(100);

    FILETIME idle2, kern2, user2;
    if (!GetSystemTimes(&idle2, &kern2, &user2)) return 0.0;

    auto toU64 = [](const FILETIME& ft) -> unsigned long long {
        return (static_cast<unsigned long long>(ft.dwHighDateTime) << 32)
             | static_cast<unsigned long long>(ft.dwLowDateTime);
    };

    const unsigned long long idleDelta = toU64(idle2) - toU64(idle1);
    const unsigned long long kernDelta = toU64(kern2) - toU64(kern1);
    const unsigned long long userDelta = toU64(user2) - toU64(user1);

    const unsigned long long total = kernDelta + userDelta;
    if (total == 0) return 0.0;

    const double busy = static_cast<double>(total - idleDelta);
    return (busy / static_cast<double>(total)) * 100.0;
#else
    return 0.0;
#endif
}

// ---------------------------------------------------------------------------
// Memory: GlobalMemoryStatusEx
// ---------------------------------------------------------------------------
static void memoryInfo(unsigned long long& total, unsigned long long& used, double& percent)
{
    total = 0; used = 0; percent = 0.0;
#ifdef _WIN32
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        total = ms.ullTotalPhys;
        used = total - ms.ullAvailPhys;
        percent = ms.dwMemoryLoad;
    }
#else
    (void)total; (void)used; (void)percent;
#endif
}

// ---------------------------------------------------------------------------
// Disk: GetDiskFreeSpaceEx on the drive that hosts the vault / working dir.
// ---------------------------------------------------------------------------
static void diskInfo(unsigned long long& total, unsigned long long& used, double& percent)
{
    total = 0; used = 0; percent = 0.0;
#ifdef _WIN32
    ULARGE_INTEGER freeBytes{};
    ULARGE_INTEGER totalBytes{};
    if (GetDiskFreeSpaceExW(L"C:\\", &freeBytes, &totalBytes, nullptr) && totalBytes.QuadPart > 0) {
        total = totalBytes.QuadPart;
        used = total - freeBytes.QuadPart;
        percent = (static_cast<double>(used) / static_cast<double>(total)) * 100.0;
    }
#else
    (void)total; (void)used; (void)percent;
#endif
}

void HealthHandler::handle(const char* /*request*/, void* response)
{
    auto* res = static_cast<httplib::Response*>(response);

    const double cpu = cpuUsagePercent();

    unsigned long long memTotal = 0, memUsed = 0;
    double memPercent = 0.0;
    memoryInfo(memTotal, memUsed, memPercent);

    unsigned long long diskTotal = 0, diskUsed = 0;
    double diskPercent = 0.0;
    diskInfo(diskTotal, diskUsed, diskPercent);

    const unsigned long long uptimeSec =
        static_cast<unsigned long long>(GetTickCount64() / 1000ULL);

    std::ostringstream json;
    json << "{";
    json << "\"cpu_usage\":" << cpu;
    json << ",\"memory_total\":" << memTotal;
    json << ",\"memory_used\":" << memUsed;
    json << ",\"memory_percent\":" << memPercent;
    json << ",\"disk_total\":" << diskTotal;
    json << ",\"disk_used\":" << diskUsed;
    json << ",\"disk_percent\":" << diskPercent;
    json << ",\"uptime\":" << uptimeSec;
    json << "}";

    res->set_content(json.str(), "application/json; charset=utf-8");
}
