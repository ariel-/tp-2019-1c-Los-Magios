#include "AppenderConsole.h"
#include "Appender.h"
#include "LogMessage.h"
#include "Malloc.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    Appender _base;

    ColorTypes colors[NUM_ENABLED_LOG_LEVELS];
} AppenderConsole;

static void _write(Appender* appender, LogMessage* message);
static void _destroy(Appender* appender);

Appender* AppenderConsole_Create(LogLevel level, AppenderFlags flags, ColorTypes traceColor, ColorTypes debugColor,
                                 ColorTypes infoColor, ColorTypes warnColor, ColorTypes errColor, ColorTypes fatalColor)
{
    AppenderConsole* me = Malloc(sizeof(AppenderConsole));
    Appender_Init(&me->_base, level, flags, _write, _destroy);

    for (uint8_t i = 0; i < NUM_ENABLED_LOG_LEVELS; ++i)
        me->colors[i] = MAX;

    me->colors[LOG_LEVEL_TRACE - 1] = traceColor;
    me->colors[LOG_LEVEL_DEBUG - 1] = debugColor;
    me->colors[LOG_LEVEL_INFO - 1] = infoColor;
    me->colors[LOG_LEVEL_WARN - 1] = warnColor;
    me->colors[LOG_LEVEL_ERROR - 1] = errColor;
    me->colors[LOG_LEVEL_FATAL - 1] = fatalColor;

    return (Appender*) me;
}

static void _write(Appender* appender, LogMessage* message)
{
    AppenderConsole* me = (AppenderConsole*) appender;
    FILE* const out = (message->Level < LOG_LEVEL_ERROR) ? stdout : stderr;

    enum ANSITextAttr
    {
        TA_NORMAL = 0, TA_BOLD = 1, TA_BLINK = 5, TA_REVERSE = 7
    };

    enum ANSIFgTextAttr
    {
        FG_BLACK = 30, FG_RED, FG_GREEN, FG_BROWN, FG_BLUE, FG_MAGENTA, FG_CYAN, FG_WHITE, FG_YELLOW
    };

    enum ANSIBgTextAttr
    {
        BG_BLACK = 40, BG_RED, BG_GREEN, BG_BROWN, BG_BLUE, BG_MAGENTA, BG_CYAN, BG_WHITE
    };

    static uint8_t UnixColorFG[MAX] =
    {
        FG_BLACK,    // BLACK
        FG_RED,      // RED
        FG_GREEN,    // GREEN
        FG_BROWN,    // BROWN
        FG_BLUE,     // BLUE
        FG_MAGENTA,  // MAGENTA
        FG_CYAN,     // CYAN
        FG_WHITE,    // WHITE
        FG_YELLOW,   // YELLOW
        FG_RED,      // LRED
        FG_GREEN,    // LGREEN
        FG_BLUE,     // LBLUE
        FG_MAGENTA,  // LMAGENTA
        FG_CYAN,     // LCYAN
        FG_WHITE     // LWHITE
    };

    uint8_t index;
    switch (message->Level)
    {
        case LOG_LEVEL_TRACE:
        case LOG_LEVEL_DEBUG:
        case LOG_LEVEL_INFO:
        case LOG_LEVEL_WARN:
        case LOG_LEVEL_FATAL:
            index = message->Level - 1;
            break;
        case LOG_LEVEL_ERROR: // No break on purpose
        default:
            index = LOG_LEVEL_ERROR - 1;
            break;
    }

    ColorTypes const color = me->colors[index];
    fprintf(out, "\x1b[%d%sm%s%s\x1b[0m\n", UnixColorFG[color], (color >= YELLOW && color < MAX ? ";1" : ""),
            message->Prefix, message->Text);
}

static void _destroy(Appender* appender)
{
    AppenderConsole* me = (AppenderConsole*) appender;
    Free(me);
}
