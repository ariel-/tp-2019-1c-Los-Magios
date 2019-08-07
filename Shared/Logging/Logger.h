
#ifndef Logger_h__
#define Logger_h__

#include "LogCommon.h"
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <vector.h>

typedef struct Appender Appender;
typedef struct LogMessage LogMessage;

void Logger_Init(void);
void Logger_AddAppender(Appender* appender);
void Logger_DelAppenders(void);
void Logger_Write(LogMessage* message);
void Logger_Format(LogLevel level, char const* format, ...);
char const* Logger_GetLogTimeStampStr(void);
void Logger_Terminate(void);

// este voodoo chequea strings de formato en tiempo de compilación :)
void check_args(char const*, ...) __attribute__ ((__format__ (__printf__, 1, 2)));

#define LISSANDRA_LOG_MESSAGE(level__, ...)  \
    do                                       \
    {                                        \
        if (false)                           \
            check_args(__VA_ARGS__);         \
                                             \
        Logger_Format(level__, __VA_ARGS__); \
    } while(false)

#define LISSANDRA_LOG_TRACE(...) \
    LISSANDRA_LOG_MESSAGE(LOG_LEVEL_TRACE, __VA_ARGS__)

#define LISSANDRA_LOG_INFO(...)  \
    LISSANDRA_LOG_MESSAGE(LOG_LEVEL_INFO, __VA_ARGS__)

#define LISSANDRA_LOG_DEBUG(...) \
    LISSANDRA_LOG_MESSAGE(LOG_LEVEL_DEBUG, __VA_ARGS__)

#define LISSANDRA_LOG_WARN(...)  \
    LISSANDRA_LOG_MESSAGE(LOG_LEVEL_WARN, __VA_ARGS__)

#define LISSANDRA_LOG_ERROR(...) \
    LISSANDRA_LOG_MESSAGE(LOG_LEVEL_ERROR, __VA_ARGS__)

#define LISSANDRA_LOG_FATAL(...) \
    LISSANDRA_LOG_MESSAGE(LOG_LEVEL_FATAL, __VA_ARGS__)

#define LISSANDRA_LOG_SYSERR(er__, call__)                                                  \
do                                                                                          \
{                                                                                           \
        char errmsg__[256];                                                                 \
        strerror_r(er__, errmsg__, 256);                                                    \
        LISSANDRA_LOG_ERROR("%s: Error en %s: %i (%s)", __func__, call__, er__, errmsg__);  \
} while (false)

#define LISSANDRA_LOG_SYSERROR(call__) LISSANDRA_LOG_SYSERR(errno, call__)

#endif //Logger_h__
