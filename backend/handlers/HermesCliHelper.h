#pragma once

#include <string>

// Shared CLI helper — runs `hermes` commands and captures stdout.
// Used by panel handlers that need JSON output from Hermes CLI.
struct HermesCliHelper
{
    // Run a hermes CLI command with given args. Returns stdout (or empty on error).
    static std::string run(const std::string& args);
};
