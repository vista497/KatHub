#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <fstream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <thread>

namespace KatHub {

/// Log levels.
enum class LogLevel
{
    Debug,      // Detailed debug information
    Info,       // Informational messages
    Warning,    // Warnings
    Error,      // Errors
};

/// Log output format.
enum class LogFormat
{
    Plain,      // Human-readable text
    Json,       // JSON for machine analysis
};

/// A single log entry.
struct LogEntry
{
    std::chrono::system_clock::time_point timestamp;
    LogLevel    level;
    std::string message;
    std::string source;
    std::thread::id threadId;

    std::string toPlainText() const;
    std::string toJson() const;
};

/// Log handler interface (observer pattern).
class ILogHandler
{
public:
    virtual ~ILogHandler() = default;
    virtual void handle(const LogEntry &entry) = 0;
};

/// Thread-safe file + console logger (singleton).
///
/// Usage:
///   Logger::instance().init(LogLevel::Debug, "app.log");
///   LOG_INFO("Application started");
class Logger
{
public:
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    /// Get singleton instance.
    static Logger &instance();

    /// Initialize the logger (call once at startup).
    /// @param minLevel   Minimum level to log.
    /// @param logFilePath  Optional path to log file (appended mode).
    void init(LogLevel minLevel = LogLevel::Info,
              const std::string &logFilePath = "");

    /// Log a message.
    void log(LogLevel level, const std::string &message,
             const std::string &source = "");

    // Convenience methods.
    void debug(const std::string &message, const std::string &source = "");
    void info(const std::string &message, const std::string &source = "");
    void warning(const std::string &message, const std::string &source = "");
    void error(const std::string &message, const std::string &source = "");

    /// Add a custom log handler.
    void addHandler(std::unique_ptr<ILogHandler> handler);

    /// Set minimum log level.
    void setMinLevel(LogLevel level);

    /// Set output format.
    void setFormat(LogFormat format);

    /// Enable / disable logging.
    void setEnabled(bool enabled);

    /// Get recent log entries.
    std::vector<LogEntry> getRecentLogs(size_t count = 100) const;

    /// Clear log history.
    void clearLogHistory();

    ~Logger();

private:
    Logger();

    void writeToConsole(const LogEntry &entry);
    void writeToFile(const LogEntry &entry);
    std::string formatLogEntry(const LogEntry &entry) const;

    LogLevel    m_minLevel = LogLevel::Info;
    LogFormat   m_format   = LogFormat::Plain;
    std::vector<std::unique_ptr<ILogHandler>> m_handlers;
    mutable std::mutex m_mutex;
    std::atomic<bool>  m_enabled{true};
    std::ofstream      m_fileStream;
};

// ── Logging macros ─────────────────────────────────────────────

#define LOG_DEBUG(msg) \
    KatHub::Logger::instance().debug(msg, __FILE__ ":" + std::to_string(__LINE__))
#define LOG_INFO(msg) \
    KatHub::Logger::instance().info(msg, __FILE__ ":" + std::to_string(__LINE__))
#define LOG_WARNING(msg) \
    KatHub::Logger::instance().warning(msg, __FILE__ ":" + std::to_string(__LINE__))
#define LOG_ERROR(msg) \
    KatHub::Logger::instance().error(msg, __FILE__ ":" + std::to_string(__LINE__))

} // namespace KatHub
