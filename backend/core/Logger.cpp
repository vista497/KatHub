#include "Logger.h"

#include <iostream>
#include <algorithm>
#include <ctime>
#include <filesystem>

namespace KatHub {

// ── Internal log history ───────────────────────────────────────

namespace {
    std::vector<LogEntry>  s_history;
    constexpr size_t       MAX_HISTORY = 10000;
    std::mutex             s_historyMutex;

    std::string levelToString(LogLevel level)
    {
        switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error:   return "ERROR";
        default:                return "UNKNOWN";
        }
    }
}

// ── LogEntry formatting ────────────────────────────────────────

std::string LogEntry::toPlainText() const
{
    std::time_t t = std::chrono::system_clock::to_time_t(timestamp);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " "
        << KatHub::levelToString(level) << " "
        << "[" << source << "] "
        << "[Thread:" << threadId << "] "
        << message;
    return oss.str();
}

std::string LogEntry::toJson() const
{
    std::time_t t = std::chrono::system_clock::to_time_t(timestamp);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream oss;
    oss << "{"
        << "\"timestamp\":\"" << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << "\","
        << "\"level\":\"" << KatHub::levelToString(level) << "\","
        << "\"source\":\"" << source << "\","
        << "\"message\":\"" << message << "\""
        << "}";
    return oss.str();
}

// ── Singleton ──────────────────────────────────────────────────

Logger &Logger::instance()
{
    static Logger s_instance;
    return s_instance;
}

Logger::Logger() = default;
Logger::~Logger()
{
    if (m_fileStream.is_open())
        m_fileStream.close();
}

// ── init ───────────────────────────────────────────────────────

void Logger::init(LogLevel minLevel, const std::string &logFilePath)
{
    setMinLevel(minLevel);

    if (!logFilePath.empty()) {
        std::filesystem::path p(logFilePath);
        if (p.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);
        }

        m_fileStream.open(logFilePath, std::ios::app);
        if (!m_fileStream.is_open()) {
            std::cerr << "[Logger] Failed to open log file: "
                      << logFilePath << std::endl;
        }
    }
}

// ── log ────────────────────────────────────────────────────────

void Logger::log(LogLevel level, const std::string &message,
                 const std::string &source)
{
    if (!m_enabled || level < m_minLevel)
        return;

    LogEntry entry{
        std::chrono::system_clock::now(),
        level,
        message,
        source,
        std::this_thread::get_id()
    };

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        writeToConsole(entry);

        if (m_fileStream.is_open())
            writeToFile(entry);

        for (auto &h : m_handlers)
            h->handle(entry);
    }

    // Store in history (separate lock to avoid contention with handlers)
    {
        std::lock_guard<std::mutex> hl(s_historyMutex);
        s_history.push_back(entry);
        if (s_history.size() > MAX_HISTORY) {
            s_history.erase(s_history.begin(),
                            s_history.begin()
                                + static_cast<ptrdiff_t>(s_history.size()
                                                         - MAX_HISTORY));
        }
    }
}

// ── Convenience methods ────────────────────────────────────────

void Logger::debug(const std::string &message, const std::string &source)
{
    log(LogLevel::Debug, message, source);
}

void Logger::info(const std::string &message, const std::string &source)
{
    log(LogLevel::Info, message, source);
}

void Logger::warning(const std::string &message, const std::string &source)
{
    log(LogLevel::Warning, message, source);
}

void Logger::error(const std::string &message, const std::string &source)
{
    log(LogLevel::Error, message, source);
}

// ── Console output ─────────────────────────────────────────────

void Logger::writeToConsole(const LogEntry &entry)
{
    std::string formatted;
    if (m_format == LogFormat::Json)
        formatted = entry.toJson();
    else
        formatted = entry.toPlainText();

    switch (entry.level) {
    case LogLevel::Error:
        std::cerr << "[ERROR]   " << formatted << std::endl;
        break;
    case LogLevel::Warning:
        std::cerr << "[WARNING] " << formatted << std::endl;
        break;
    default:
        std::cout << formatted << std::endl;
        break;
    }
}

// ── File output ────────────────────────────────────────────────

void Logger::writeToFile(const LogEntry &entry)
{
    if (!m_fileStream.is_open())
        return;

    std::string formatted;
    if (m_format == LogFormat::Json)
        formatted = entry.toJson();
    else
        formatted = entry.toPlainText();

    m_fileStream << formatted << std::endl;
    m_fileStream.flush();
}

// ── Handler ────────────────────────────────────────────────────

void Logger::addHandler(std::unique_ptr<ILogHandler> handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handlers.push_back(std::move(handler));
}

// ── Setters ────────────────────────────────────────────────────

void Logger::setMinLevel(LogLevel level)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_minLevel = level;
}

void Logger::setFormat(LogFormat format)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_format = format;
}

void Logger::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

// ── History ────────────────────────────────────────────────────

std::vector<LogEntry> Logger::getRecentLogs(size_t count) const
{
    std::lock_guard<std::mutex> lock(s_historyMutex);
    if (count >= s_history.size())
        return s_history;
    return std::vector<LogEntry>(s_history.end() - static_cast<ptrdiff_t>(count),
                                 s_history.end());
}

void Logger::clearLogHistory()
{
    std::lock_guard<std::mutex> lock(s_historyMutex);
    s_history.clear();
}

} // namespace KatHub
