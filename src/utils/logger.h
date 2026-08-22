/**
 * @file logger.h
 * @brief Lightweight structured logging for the UUV simulation.
 *
 * Usage:
 *     LOG_INFO("Simulation started with %d seekers", m_seekers.size());
 *     LOG_WARN("Detector %d has no sightings", d.id);
 *     LOG_ERROR("Failed to load scenario: %s", path.c_str());
 *
 * Log levels can be controlled at compile time:
 *     -DLOG_LEVEL=0  (ERROR only)
 *     -DLOG_LEVEL=1  (WARN + ERROR)
 *     -DLOG_LEVEL=2  (INFO + WARN + ERROR)
 *     -DLOG_LEVEL=3  (DEBUG + all)
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <string>

// Default log level if not specified: INFO (2)
#ifndef LOG_LEVEL
#define LOG_LEVEL 2
#endif

namespace uuv {

enum class LogLevel : int {
    DEBUG = 0,
    INFO  = 1,
    WARN  = 2,
    ERROR = 3,
};

inline const char* logLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

inline std::string logTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _MSC_VER
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
    return std::string(buf);
}

class Logger {
public:
    static Logger& instance() {
        static Logger s_instance;
        return s_instance;
    }

    void setLevel(LogLevel level) { m_level = level; }
    LogLevel getLevel() const { return m_level; }

    void log(LogLevel level, const char* file, int line, const char* fmt, ...) {
        if (level < m_level) return;

        va_list args;
        va_start(args, fmt);
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        fprintf(stderr, "[%s] [%s] %s:%d: %s\n",
                logTimestamp().c_str(),
                logLevelString(level),
                file, line, buf);
        fflush(stderr);
    }

private:
    Logger() : m_level(static_cast<LogLevel>(LOG_LEVEL)) {}
    LogLevel m_level;
};

} // namespace uuv

// Convenience macros
#define LOG_DEBUG(fmt, ...) \
    uuv::Logger::instance().log(uuv::LogLevel::DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    uuv::Logger::instance().log(uuv::LogLevel::INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    uuv::Logger::instance().log(uuv::LogLevel::WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    uuv::Logger::instance().log(uuv::LogLevel::ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif // LOGGER_H
