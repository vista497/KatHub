#include "CrashHandler.h"

#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <iostream>

#pragma comment(lib, "dbghelp.lib")

namespace KatHub {

// ── Globals ─────────────────────────────────────────────────────

std::string CrashHandler::s_logDir;
CrashHandler::PanicCallback CrashHandler::s_onPanic;
void *CrashHandler::s_handlerHandle = nullptr;

namespace {
    std::mutex s_panicMutex;

    std::string panicFlagPath()
    {
        return CrashHandler::logDir() + "/_panic.flag";
    }

    std::string panicLogPath()
    {
        return CrashHandler::logDir() + "/_panic.log";
    }

    // ── Stack walking ───────────────────────────────────────────

    std::string formatStackFrame(DWORD64 addr, const std::string &symbol,
                                 const std::string &file, int line)
    {
        std::ostringstream oss;
        oss << "    0x" << std::hex << addr << std::dec;
        if (!symbol.empty())
            oss << "  " << symbol;
        if (!file.empty()) {
            oss << "  (" << file;
            if (line > 0)
                oss << ":" << line;
            oss << ")";
        }
        return oss.str();
    }

    std::string walkStack(CONTEXT *context, HANDLE hProcess, HANDLE hThread)
    {
        SymInitialize(hProcess, nullptr, TRUE);
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);

        STACKFRAME64 frame = {};
#ifdef _WIN64
        frame.AddrPC.Offset    = context->Rip;
        frame.AddrFrame.Offset = context->Rbp;
        frame.AddrStack.Offset = context->Rsp;
        DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
#else
        frame.AddrPC.Offset    = context->Eip;
        frame.AddrFrame.Offset = context->Ebp;
        frame.AddrStack.Offset = context->Esp;
        DWORD machineType = IMAGE_FILE_MACHINE_I386;
#endif
        frame.AddrPC.Mode    = AddrModeFlat;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Mode = AddrModeFlat;

        std::ostringstream oss;
        const int MAX_FRAMES = 64;
        const int SYMBOL_BUFFER_SIZE = sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR);
        char symbolBuffer[SYMBOL_BUFFER_SIZE];
        auto *pSymbol = reinterpret_cast<SYMBOL_INFO *>(symbolBuffer);
        pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        pSymbol->MaxNameLen = MAX_SYM_NAME;

        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD displacement = 0;

        for (int i = 0; i < MAX_FRAMES; ++i) {
            BOOL ok = StackWalk64(
                machineType, hProcess, hThread,
                &frame, context,
                nullptr,
                SymFunctionTableAccess64, SymGetModuleBase64, nullptr);

            if (!ok || frame.AddrPC.Offset == 0)
                break;

            std::string symName;
            std::string fileName;
            int lineNum = 0;

            DWORD64 symDisplacement = 0;
            if (SymFromAddr(hProcess, frame.AddrPC.Offset, &symDisplacement, pSymbol)) {
                symName = pSymbol->Name;
            }

            if (SymGetLineFromAddr64(hProcess, frame.AddrPC.Offset, &displacement, &line)) {
                fileName = line.FileName;
                lineNum = static_cast<int>(line.LineNumber);
            }

            oss << formatStackFrame(frame.AddrPC.Offset, symName, fileName, lineNum) << "\n";
        }

        SymCleanup(hProcess);
        return oss.str();
    }

    // ── Exception info ─────────────────────────────────────────

    std::string exceptionCodeToString(DWORD code)
    {
        switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:         return "ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_BREAKPOINT:               return "BREAKPOINT";
        case EXCEPTION_DATATYPE_MISALIGNMENT:    return "DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DENORMAL_OPERAND:     return "FLT_DENORMAL_OPERAND";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INEXACT_RESULT:       return "FLT_INEXACT_RESULT";
        case EXCEPTION_FLT_INVALID_OPERATION:    return "FLT_INVALID_OPERATION";
        case EXCEPTION_FLT_OVERFLOW:             return "FLT_OVERFLOW";
        case EXCEPTION_FLT_STACK_CHECK:          return "FLT_STACK_CHECK";
        case EXCEPTION_FLT_UNDERFLOW:            return "FLT_UNDERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION:      return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR:            return "IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW:             return "INT_OVERFLOW";
        case EXCEPTION_INVALID_DISPOSITION:      return "INVALID_DISPOSITION";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_PRIV_INSTRUCTION:         return "PRIV_INSTRUCTION";
        case EXCEPTION_SINGLE_STEP:              return "SINGLE_STEP";
        case EXCEPTION_STACK_OVERFLOW:           return "STACK_OVERFLOW";
        case 0xE06D7363:                         return "CPP_EXCEPTION (msvc)";
        default:                                 return "UNKNOWN_0x" + std::to_string(code);
        }
    }

#ifdef _WIN64
    std::string formatRegisters(CONTEXT *ctx)
    {
        std::ostringstream oss;
        oss << "    RAX=0x" << std::hex << ctx->Rax
            << " RBX=0x" << ctx->Rbx
            << " RCX=0x" << ctx->Rcx
            << " RDX=0x" << ctx->Rdx << "\n";
        oss << "    RSI=0x" << ctx->Rsi
            << " RDI=0x" << ctx->Rdi
            << " RSP=0x" << ctx->Rsp
            << " RBP=0x" << ctx->Rbp << "\n";
        oss << "    RIP=0x" << ctx->Rip
            << " R8 =0x" << ctx->R8
            << " R9 =0x" << ctx->R9
            << " R10=0x" << ctx->R10 << "\n";
        oss << "    R11=0x" << ctx->R11
            << " R12=0x" << ctx->R12
            << " R13=0x" << ctx->R13
            << " R14=0x" << ctx->R14
            << " R15=0x" << ctx->R15;
        return oss.str();
    }
#else
    std::string formatRegisters(CONTEXT *ctx)
    {
        std::ostringstream oss;
        oss << "    EAX=0x" << std::hex << ctx->Eax
            << " EBX=0x" << ctx->Ebx
            << " ECX=0x" << ctx->Ecx
            << " EDX=0x" << ctx->Edx << "\n";
        oss << "    ESI=0x" << ctx->Esi
            << " EDI=0x" << ctx->Edi
            << " ESP=0x" << ctx->Esp
            << " EBP=0x" << ctx->Ebp << "\n";
        oss << "    EIP=0x" << ctx->Eip;
        return oss.str();
    }
#endif

    // ── Process memory info ────────────────────────────────────

    std::string getMemoryInfo()
    {
        MEMORYSTATUSEX mem = {};
        mem.dwLength = sizeof(mem);
        GlobalMemoryStatusEx(&mem);

        PROCESS_MEMORY_COUNTERS pmc = {};
        pmc.cb = sizeof(pmc);
        GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));

        std::ostringstream oss;
        oss << "    System RAM: "
            << (mem.ullTotalPhys / (1024 * 1024)) << " MB total, "
            << (mem.ullAvailPhys / (1024 * 1024)) << " MB avail ("
            << mem.dwMemoryLoad << "% used)\n";
        oss << "    Process: "
            << (pmc.WorkingSetSize / 1024) << " KB working set, "
            << (pmc.PagefileUsage / 1024) << " KB pagefile";
        return oss.str();
    }

    // ── File helpers ───────────────────────────────────────────

    void writeFlagFile(const std::string &path)
    {
        std::ofstream f(path);
        if (f.is_open())
            f.close();
    }

    // ── SEH handler ────────────────────────────────────────────

    static LONG WINAPI vectoredHandler(EXCEPTION_POINTERS *exInfo)
    {
        std::lock_guard<std::mutex> lock(s_panicMutex);

        HANDLE hProcess = GetCurrentProcess();
        HANDLE hThread  = GetCurrentThread();

        // Skip non-fatal exceptions — debug events, thread naming, etc.
        DWORD code = exInfo->ExceptionRecord->ExceptionCode;
        switch (code) {
        case 0x4001000A:  // DBG_PRINTEXCEPTION_C — Qt debug output
        case 0x406D1388:  // MS_VC_EXCEPTION — thread naming (NVIDIA/Qt)
            return EXCEPTION_CONTINUE_SEARCH;  // Non-fatal, pass through
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_STACK_OVERFLOW:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_INVALID_DISPOSITION:
        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        case EXCEPTION_PRIV_INSTRUCTION:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_INT_OVERFLOW:
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        case EXCEPTION_DATATYPE_MISALIGNMENT:
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        case EXCEPTION_FLT_OVERFLOW:
        case EXCEPTION_FLT_UNDERFLOW:
        case EXCEPTION_FLT_INVALID_OPERATION:
            break;  // Fatal — log and die
        default:
            return EXCEPTION_CONTINUE_SEARCH;  // Not our problem
        }

        // Build panic info
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &t);

        std::ostringstream panic;
        panic << "=== KATHUB CRASH REPORT ===\n";
        panic << "Time:    " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "\n";
        panic << "Process: " << GetCurrentProcessId() << "\n";
        panic << "Thread:  " << GetCurrentThreadId() << "\n";
        panic << "Signal:  " << exceptionCodeToString(exInfo->ExceptionRecord->ExceptionCode)
              << " (0x" << std::hex << exInfo->ExceptionRecord->ExceptionCode << std::dec << ")\n";

        if (exInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
            ULONG_PTR operation = exInfo->ExceptionRecord->ExceptionInformation[0];
            ULONG_PTR addr      = exInfo->ExceptionRecord->ExceptionInformation[1];
            panic << "Access:  " << (operation == 0 ? "READ" :
                                      operation == 1 ? "WRITE" : "EXECUTE")
                  << " at address 0x" << std::hex << addr << std::dec << "\n";
        }

        panic << "\nRegisters:\n" << formatRegisters(exInfo->ContextRecord) << "\n";
        panic << "\nMemory:\n" << getMemoryInfo() << "\n";
        panic << "\nStack trace:\n" << walkStack(exInfo->ContextRecord, hProcess, hThread) << "\n";
        panic << "=== END CRASH REPORT ===\n";

        // Write panic log file
        try {
            std::filesystem::create_directories(CrashHandler::logDir());
            std::ofstream f(panicLogPath(), std::ios::app);
            if (f.is_open()) {
                f << panic.str() << "\n";
                f.flush();
                f.close();
            }
            writeFlagFile(panicFlagPath());
        } catch (...) {
            // Can't do much — we're crashing
        }

        // Call user callback (e.g., send to EventBus/WS before dying)
        const auto &cb = CrashHandler::panicCallback();
        if (cb) {
            try {
                cb(panic.str());
            } catch (...) {}
        }

        // Write to file only — avoid console spam for non-fatal catches
        // std::cerr << "\n" << panic.str() << "\n";

        return EXCEPTION_EXECUTE_HANDLER;
    }

} // anonymous namespace

// ── Public API ──────────────────────────────────────────────────

bool CrashHandler::install(const std::string &logDir,
                            PanicCallback onPanic)
{
    s_logDir  = logDir;
    s_onPanic = std::move(onPanic);

    s_handlerHandle = AddVectoredExceptionHandler(1, vectoredHandler);
    return s_handlerHandle != nullptr;
}

void CrashHandler::uninstall()
{
    if (s_handlerHandle) {
        RemoveVectoredExceptionHandler(s_handlerHandle);
        s_handlerHandle = nullptr;
    }
}

bool CrashHandler::hadPreviousCrash()
{
    return std::filesystem::exists(panicFlagPath());
}

std::string CrashHandler::readPanicLog()
{
    std::string logPath = panicLogPath();
    if (!std::filesystem::exists(logPath))
        return {};

    std::ifstream f(logPath);
    if (!f.is_open())
        return {};

    std::ostringstream oss;
    oss << f.rdbuf();
    return oss.str();
}

void CrashHandler::clearPanicFlag()
{
    try {
        std::filesystem::remove(panicFlagPath());
    } catch (...) {}
}

void CrashHandler::writePanicLog(const std::string &reason)
{
    writeFlagFile(panicFlagPath());

    try {
        std::filesystem::create_directories(s_logDir);
        std::ofstream f(panicLogPath(), std::ios::app);
        if (f.is_open()) {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
            localtime_s(&tm, &t);

            f << "=== MANUAL PANIC ===\n";
            f << "Time: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "\n";
            f << "Reason: " << reason << "\n";
            f << "=== END MANUAL PANIC ===\n\n";
            f.flush();
        }
    } catch (...) {}
}

} // namespace KatHub
