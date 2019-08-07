
#include "LogMessage.h"
#include "Malloc.h"
#include <libcommons/string.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

LogMessage* LogMessage_Init(LogLevel level, char const* text)
{
    LogMessage* m = Malloc(sizeof(LogMessage));
    m->Level = level;
    m->Prefix = NULL;
    m->Text = string_duplicate(text);
    timespec_get(&m->MsgTime, TIME_UTC);
    return m;
}

size_t LogMessage_Size(LogMessage const* logMessage)
{
    return strlen(logMessage->Prefix) + strlen(logMessage->Text);
}

void LogMessage_GetTimeStr(LogMessage const* logMessage, char* result)
{
    GetTimeStr(&logMessage->MsgTime, result);
}

void GetTimeStr(struct timespec const* ts, char* result)
{
    struct tm local;
    localtime_r(&ts->tv_sec, &local);

    //YYYY-MM-DD_hh:mm:ss.nnn
    snprintf(result, 23 + 1, "%04d-%02d-%02d_%02d:%02d:%02d.%03lu", local.tm_year + 1900, local.tm_mon + 1,
             local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec, ts->tv_nsec);
}

void LogMessage_Destroy(LogMessage* msg)
{
    Free(msg->Prefix);
    Free(msg->Text);
    Free(msg);
}
