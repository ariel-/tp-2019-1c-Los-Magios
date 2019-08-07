
#include "AppenderFile.h"
#include "Appender.h"
#include "Logger.h"
#include "LogMessage.h"
#include "Malloc.h"
#include <libcommons/string.h>
#include <linux/limits.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    Appender _base;

    FILE* logFile;
    char* fileName;
    bool backup;
    size_t maxFileSize;
    atomic_size_t fileSize;
} AppenderFile;

static FILE* _openFile(AppenderFile* appender, char const* name, char const* mode, bool backup);
static void _closeFile(AppenderFile* appender);

static void _write(Appender* appender, LogMessage* message);
static void _destroy(Appender* appender);

Appender* AppenderFile_Create(LogLevel level, AppenderFlags flags, char const* fileName, char const* fileMode,
                              size_t maxSize)
{
    AppenderFile* me = Malloc(sizeof(AppenderFile));
    Appender_Init(&me->_base, level, flags, _write, _destroy);

    me->logFile = NULL;
    me->maxFileSize = 0;
    atomic_init(&me->fileSize, 0);

    me->fileName = string_duplicate(fileName);
    char const* mode = "a";
    if (fileMode)
        mode = fileMode;

    if (flags & APPENDER_FLAGS_USE_TIMESTAMP)
    {
        char* dotPos = strrchr(fileName, '.');
        char const* timeStampStr = Logger_GetLogTimeStampStr();
        if (dotPos == NULL)
            string_append(&me->fileName, timeStampStr);
        else
        {
            Free(me->fileName);
            size_t pos = dotPos - fileName;
            char* name = string_substring_until(fileName, pos);
            char* ext = string_substring_from(fileName, pos + 1);

            me->fileName = string_from_format("%s_%s.%s", name, timeStampStr, ext);

            Free(ext);
            Free(name);
        }
    }

    if (maxSize)
        me->maxFileSize = maxSize;

    me->backup = !!(flags & APPENDER_FLAGS_MAKE_FILE_BACKUP);
    me->logFile = _openFile(me, me->fileName, mode, !strcmp(mode, "w") && me->backup);
    return (Appender*) me;
}

static FILE* _openFile(AppenderFile* appender, char const* name, char const* mode, bool backup)
{
    if (backup)
    {
        _closeFile(appender);

        char* newName = string_duplicate(name);
        {
            char timeStr[23 + 1];
            struct timespec time;
            timespec_get(&time, TIME_UTC);
            GetTimeStr(&time, timeStr);
            string_append_with_format(&newName, ".%s", timeStr);
        }

        string_replace(newName, ':', '-');
        rename(name, newName);
        Free(newName);
    }

    FILE* ret = fopen(name, mode);
    if (!ret)
        return NULL;

    atomic_store(&appender->fileSize, ftell(ret));
    return ret;
}

static void _closeFile(AppenderFile* appender)
{
    if (appender->logFile)
    {
        fclose(appender->logFile);
        appender->logFile = NULL;
    }
}

static void _write(Appender* appender, LogMessage* message)
{
    AppenderFile* me = (AppenderFile*) appender;
    bool exceedMaxSize =
            me->maxFileSize > 0 && (atomic_load(&me->fileSize) + LogMessage_Size(message)) > me->maxFileSize;

    if (exceedMaxSize)
        me->logFile = _openFile(me, me->fileName, "w", true);

    if (!me->logFile)
        return;

    fprintf(me->logFile, "%s%s\n", message->Prefix, message->Text);
    fflush(me->logFile);
    me->fileSize += LogMessage_Size(message);
}

static void _destroy(Appender* appender)
{
    AppenderFile* me = (AppenderFile*) appender;
    _closeFile(me);
    Free(me->fileName);
    Free(me);
}
