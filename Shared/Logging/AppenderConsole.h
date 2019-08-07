
#ifndef AppenderConsole_h__
#define AppenderConsole_h__

#include "LogCommon.h"

typedef enum
{
    BLACK,
    RED,
    GREEN,
    BROWN,
    BLUE,
    MAGENTA,
    CYAN,
    GREY,
    YELLOW,
    LRED,
    LGREEN,
    LBLUE,
    LMAGENTA,
    LCYAN,
    WHITE,
    MAX
} ColorTypes;

typedef struct Appender Appender;

Appender* AppenderConsole_Create(LogLevel level, AppenderFlags flags, ColorTypes traceColor, ColorTypes debugColor,
                                 ColorTypes infoColor, ColorTypes warnColor, ColorTypes errColor,
                                 ColorTypes fatalColor);

#endif //AppenderConsole_h__
