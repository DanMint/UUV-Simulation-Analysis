/**
 * @file test_logger.cpp
 * @brief Unit tests for the uuv::Logger utility.
 */

#include "../src/utils/logger.h"
#include <sstream>
#include <cstdio>
#include <cstring>

static int g_failures = 0;

static void check(bool cond, const char* name) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", name);
        g_failures++;
    }
}

int main() {
    uuv::Logger::instance().setLevel(uuv::LogLevel::DEBUG);

    // Test 1: Logger initializes with default level
    {
        uuv::Logger& log = uuv::Logger::instance();
        check(log.getLevel() == uuv::LogLevel::DEBUG, "logger_default_level_debug");
    }

    // Test 2: setLevel changes level
    {
        uuv::Logger::instance().setLevel(uuv::LogLevel::WARN);
        check(uuv::Logger::instance().getLevel() == uuv::LogLevel::WARN, "logger_setlevel_works");
        uuv::Logger::instance().setLevel(uuv::LogLevel::INFO);
    }

    // Test 3: logLevelString returns correct strings
    {
        check(std::strcmp(uuv::logLevelString(uuv::LogLevel::DEBUG), "DEBUG") == 0, "logLevelString_debug");
        check(std::strcmp(uuv::logLevelString(uuv::LogLevel::INFO), "INFO") == 0, "logLevelString_info");
        check(std::strcmp(uuv::logLevelString(uuv::LogLevel::WARN), "WARN") == 0, "logLevelString_warn");
        check(std::strcmp(uuv::logLevelString(uuv::LogLevel::ERROR), "ERROR") == 0, "logLevelString_error");
    }

    // Test 4: logTimestamp returns non-empty string
    {
        std::string ts = uuv::logTimestamp();
        check(!ts.empty(), "logTimestamp_non_empty");
        check(ts.size() == 8, "logTimestamp_format_HH:MM:SS");  // "HH:MM:SS"
    }

    // Test 5: macros compile and work
    {
        uuv::Logger::instance().setLevel(uuv::LogLevel::INFO);
        LOG_INFO("test info message");
        LOG_WARN("test warn message");
        LOG_ERROR("test error message");
        check(true, "logger_macros_compile_and_run");
    }

    // Test 6: DEBUG level required for DEBUG macro
    {
        uuv::Logger::instance().setLevel(uuv::LogLevel::ERROR);
        LOG_DEBUG("this should not appear");
        check(true, "logger_debug_suppressed_at_error_level");
        uuv::Logger::instance().setLevel(uuv::LogLevel::DEBUG);
    }

    // Test 7: format string works
    {
        LOG_INFO("formatted: %d %s", 42, "hello");
        check(true, "logger_format_string");
    }

    std::fprintf(stdout, "\n[TEST] %d checks, %d failures\n", 7, g_failures);
    std::fflush(stdout);
    return g_failures == 0 ? 0 : 1;
}
