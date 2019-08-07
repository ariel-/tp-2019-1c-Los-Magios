
#ifndef Appender_h__
#define Appender_h__

#include "LogCommon.h"
#include <stdint.h>

typedef struct Appender Appender;
typedef struct LogMessage LogMessage;

typedef void(*AppenderWriteFn)(Appender* appender, LogMessage* message);
typedef void(*AppenderDestroyFn)(Appender* appender);

typedef struct Appender
{
    LogLevel Level;
    AppenderFlags Flags;

    // Polymorphism stuff
    AppenderWriteFn _write;
    AppenderDestroyFn _destroy;
} Appender;

void Appender_Init(Appender* a, LogLevel level, AppenderFlags flags, AppenderWriteFn writeFn, AppenderDestroyFn destroyFn);

LogLevel Appender_GetLogLevel(Appender const* appender);
AppenderFlags Appender_GetFlags(Appender const* appender);

void Appender_SetLogLevel(Appender* appender, LogLevel level);
void Appender_Write(Appender* appender, LogMessage* message);

void Appender_Destroy(Appender* appender);

char const* GetLogLevelString(LogLevel level);

#endif //Appender_h__
