
#ifndef LogMessage_h__
#define LogMessage_h__

#include "LogCommon.h"
#include <time.h>

typedef struct LogMessage
{
    LogLevel Level;
    char* Prefix;
    char* Text;
    struct timespec MsgTime;
} LogMessage;

LogMessage* LogMessage_Init(LogLevel level, char const* text);
size_t LogMessage_Size(LogMessage const* logMessage);
void LogMessage_GetTimeStr(LogMessage const* logMessage, char* result);
void LogMessage_Destroy(LogMessage* msg);

void GetTimeStr(struct timespec const* ts, char* result);

#endif //LogMessage_h__
