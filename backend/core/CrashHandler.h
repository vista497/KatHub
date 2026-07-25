#pragma once

#include <string>
#include <functional>

namespace KatHub {

/// Windows crash handler with stack trace and self-diagnosis.
///
/// Installs a VectoredExceptionHandler that:
///   - Writes a panic log with exception code, registers, stack trace
///   - Calls optional custom callback before termination
///   - Restart detection: on next startup, detects previous crash
///
/// Usage:
///   CrashHandler::install("crash.log");
///   // ... application runs ...
///   if (CrashHandler::hadPreviousCrash()) {
///       auto report = CrashHandler::readPanicLog();
///       // Log and/or send report
///   }
class CrashHandler
{
public:
    /// Callback invoked right before the process dies.
    using PanicCallback = std::function<void(const std::string &panicInfo)>;

    /// Install the crash handler.
    /// @param logDir  Directory for panic logs (e.g. "%APPDATA%/KatHub/logs")
    /// @param onPanic Optional callback invoked before termination.
    /// @return true if installed successfully.
    static bool install(const std::string &logDir,
                        PanicCallback onPanic = {});

    /// Uninstall the crash handler.
    static void uninstall();

    /// Check if the previous run ended with a crash.
    static bool hadPreviousCrash();

    /// Read the panic log from the previous crash run (empty if no crash).
    static std::string readPanicLog();

    /// Delete the panic flag and log file after a successful startup.
    static void clearPanicFlag();

    /// Manually write a panic log (e.g., for std::terminate or unhandled C++ exceptions).
    static void writePanicLog(const std::string &reason);

    // Internal: called by SEH handler to get log dir and panic callback.
    static const std::string &logDir() { return s_logDir; }
    static const PanicCallback &panicCallback() { return s_onPanic; }

private:
    CrashHandler() = delete;

    static std::string s_logDir;
    static PanicCallback s_onPanic;
    static void *s_handlerHandle;
};

} // namespace KatHub
