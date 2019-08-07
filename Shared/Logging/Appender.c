
#include "Appender.h"
#include "LogMessage.h"
#include "Malloc.h"
#include <libcommons/string.h>
#include <string.h>

void Appender_Init(Appender* a, LogLevel level, AppenderFlags flags, AppenderWriteFn writeFn, AppenderDestroyFn destroyFn)
{
    a->Level = level;
    a->Flags = flags;

    a->_write = writeFn;
    a->_destroy = destroyFn;
}

LogLevel Appender_GetLogLevel(Appender const* appender)
{
    return appender->Level;
}

AppenderFlags Appender_GetFlags(Appender const* appender)
{
    return appender->Flags;
}

void Appender_SetLogLevel(Appender* appender, LogLevel level)
{
    appender->Level = level;
}

void Appender_Write(Appender* appender, LogMessage* message)
{
    if (!appender->Level || appender->Level > message->Level)
        return;

    Free(message->Prefix);
    message->Prefix = string_new();
    if (appender->Flags & APPENDER_FLAGS_PREFIX_TIMESTAMP)
    {
        char timestamp[23 + 1];
        LogMessage_GetTimeStr(message, timestamp);
        string_append_with_format(&message->Prefix, "[%s] ", timestamp);
    }

    if (appender->Flags & APPENDER_FLAGS_PREFIX_LOGLEVEL)
        string_append_with_format(&message->Prefix, "%-5s: ", GetLogLevelString(message->Level));

    appender->_write(appender, message);
}

void Appender_Destroy(Appender* appender)
{
    appender->_destroy(appender);
}

char const* GetLogLevelString(LogLevel level)
{
    switch (level)
    {
        case LOG_LEVEL_FATAL:
            return "FATAL";
        case LOG_LEVEL_ERROR:
            return "ERROR";
        case LOG_LEVEL_WARN:
            return "WARN";
        case LOG_LEVEL_INFO:
            return "INFO";
        case LOG_LEVEL_DEBUG:
            return "DEBUG";
        case LOG_LEVEL_TRACE:
            return "TRACE";
        default:
            return "DISABLED";
    }
}
