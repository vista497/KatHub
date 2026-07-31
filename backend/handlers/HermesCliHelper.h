#pragma once

#include <string>
#include <vector>

// Shared CLI helper — runs `hermes` commands and captures stdout.
// Used by panel handlers that need JSON output from Hermes CLI.
struct HermesCliHelper
{
    // Run a hermes CLI command with given args. Returns stdout (or empty on error).
    static std::string run(const std::string& args);

    // Run a Python script with the venv interpreter that owns the hermes CLI.
    // scriptPath — absolute path to the .py file; args — positional args for it.
    static std::string runPython(const std::string& scriptPath, const std::vector<std::string>& args);

    // Full result of a CLI invocation.
    struct CliResult
    {
        std::string stdoutText;
        std::string stderrText;
        int exitCode = -1;
        bool timedOut = false;
    };

    // Run `hermes <args...>` with explicit argv (no space-splitting — safe for
    // messages containing spaces/quotes) and a configurable timeout.
    // timeoutMs < 0 waits indefinitely (used for chat runs that can take minutes).
    static CliResult runArgv(const std::vector<std::string>& args,
                             int timeoutMs = 180000);

    // Resolve the `hermes` executable:
    //   1. HERMES_EXE env var (explicit override)
    //   2. PATH lookup
    //   3. `where hermes` (first hit)
    //   4. %LOCALAPPDATA%/hermes/hermes-agent/venv/Scripts/hermes.exe
    //      (installed KatHub may run with hermes NOT in PATH)
    static std::string findHermesExe();
};
