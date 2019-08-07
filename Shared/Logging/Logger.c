
#include "Logger.h"
#include "Appender.h"
#include "LogMessage.h"
#include "Malloc.h"
#include <libcommons/string.h>
#include <stdio.h>
#include <string.h>

static struct
{
    Vector Appenders;
    char TimeStampStr[23 + 1];
} sLogger;

void _vectorFreeFn(void* elem)
{
    Appender* appender = *((Appender**) elem);
    Appender_Destroy(appender);
}

void Logger_Init(void)
{
    Vector_Construct(&sLogger.Appenders, sizeof(Appender*), _vectorFreeFn, 0);

    struct timespec time;
    timespec_get(&time, TIME_UTC);
    GetTimeStr(&time, sLogger.TimeStampStr);
}

void Logger_AddAppender(Appender* appender)
{
    Vector_push_back(&sLogger.Appenders, &appender);
}

void Logger_DelAppenders(void)
{
    Vector_clear(&sLogger.Appenders);
}

void _writeWrapper(void* elem, void* data)
{
    Appender* appender = *((Appender**) elem);
    LogMessage* msg = (LogMessage*) data;
    Appender_Write(appender, msg);
}

void Logger_Write(LogMessage* message)
{
    if (*message->Text == '\0')
        return;

    Vector_iterate_with_data(&sLogger.Appenders, _writeWrapper, message);
    LogMessage_Destroy(message);
}

void Logger_Format(LogLevel level, char const* format, ...)
{
    va_list v;
    va_start(v, format);
    char* text = string_from_vformat(format, v);
    va_end(v);

    LogMessage* msg = LogMessage_Init(level, text);
    Free(text);

    Logger_Write(msg);
}

char const* Logger_GetLogTimeStampStr(void)
{
    return sLogger.TimeStampStr;
}

void Logger_Terminate(void)
{
    Vector_Destruct(&sLogger.Appenders);
}
