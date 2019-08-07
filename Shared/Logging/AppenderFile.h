
#ifndef AppenderFile_h__
#define AppenderFile_h__

#include "LogCommon.h"
#include <stddef.h>

typedef struct Appender Appender;

Appender* AppenderFile_Create(LogLevel level, AppenderFlags flags, char const* fileName, char const* fileMode,
                              size_t maxSize);

#endif //AppenderFile_h__
