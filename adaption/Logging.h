#pragma once

#include <cstdint>
#include <cassert>
#include <string>

enum class LogLevel : uint32_t
{
    // EMERG = 0u,
    // ALERT = 1u,
    // CRIT = 2u,
    FATAL = 0u,
    FAULT = 3u,  // ERROR
    WARN = 4u,
    NOTICE = 5u,
    INFO = 6u,
    DEBUG = 7u,
    MAX = 0xFFFFu,
};

typedef void (*logging)(int level, const char* message, unsigned int length);
void SetLoggingFunc(logging func);
void LoggingOut(LogLevel level, const std::string& message);

#ifdef _HAS_FMT
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <fmt/xchar.h>

#define HAS_FORMAT 1

template <typename... Args>
inline void LoggingMessage(LogLevel level, const char* fmt, Args&&... args)
{
    try
    {
        const std::string strMessage = fmt::format(fmt, args...);
        LoggingOut(level, strMessage);
    }
    catch (const fmt::format_error& e)
    {
        LoggingOut(LogLevel::FAULT, e.what());
        assert(false && "Format log message failed! please check your format and params.");
    }
    catch (const std::exception& e)
    {
        (void)e;
        assert(false && "Print/Write log message failed!");
    }
}
#else
#define LoggingMessage(...)
#endif

#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

#ifdef HAS_FORMAT

#define LOG_ERROR_LOC(fmt, ...) LoggingMessage(LogLevel::FAULT, "#FFmpegCodec " fmt "\n>" __FILE__ "(" STRINGIZE(__LINE__) ", 0)", ##__VA_ARGS__)
#define LOG_WARNING_LOC(fmt, ...) LoggingMessage(LogLevel::WARN, "#FFmpegCodec " fmt "\n>" __FILE__ "(" STRINGIZE(__LINE__) ", 0)", ##__VA_ARGS__)
#define LOG_NOTICE_LOC(fmt, ...) LoggingMessage(LogLevel::NOTICE, "#FFmpegCodec " fmt "\n>" __FILE__ "(" STRINGIZE(__LINE__) ", 0)", ##__VA_ARGS__)
#define LOG_INFO_LOC(fmt, ...) LoggingMessage(LogLevel::INFO, "#FFmpegCodec " fmt "\n>" __FILE__ "(" STRINGIZE(__LINE__) ", 0)", ##__VA_ARGS__)
#define LOG_DEBUG_LOC(fmt, ...) LoggingMessage(LogLevel::DEBUG, "#FFmpegCodec " fmt "\n>" __FILE__ "(" STRINGIZE(__LINE__) ", 0)", ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) LOG_ERROR_LOC(fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) LoggingMessage(LogLevel::WARN, "#FFmpegCodec " fmt, ##__VA_ARGS__)
#define LOG_NOTICE(fmt, ...) LoggingMessage(LogLevel::NOTICE, "#FFmpegCodec " fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) LoggingMessage(LogLevel::INFO, "#FFmpegCodec " fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LoggingMessage(LogLevel::DEBUG, "#FFmpegCodec " fmt, ##__VA_ARGS__)

#else

#define LOG_ERROR_LOC(fmt, ...)
#define LOG_WARNING_LOC(fmt, ...)
#define LOG_NOTICE_LOC(fmt, ...)
#define LOG_INFO_LOC(fmt, ...)
#define LOG_DEBUG_LOC(fmt, ...)

#define LOG_ERROR(fmt, ...)
#define LOG_WARNING(fmt, ...)
#define LOG_NOTICE(fmt, ...)
#define LOG_INFO(fmt, ...)
#define LOG_DEBUG(fmt, ...)

#endif