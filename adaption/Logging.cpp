#include "Logging.h"

static logging s_pLogging = nullptr;

void SetLoggingFunc(logging func)
{
    s_pLogging = func;
}

void LoggingOut(LogLevel level, const std::string& message)
{
    if (s_pLogging)
    {
        s_pLogging(static_cast<int>(level), message.c_str(), static_cast<unsigned int>(message.size()));
    }
}
