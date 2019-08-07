
#include "FileWatcher.h"
#include "FDIImpl.h"
#include "Logger.h"
#include "Malloc.h"
#include <limits.h>
#include <sys/inotify.h>
#include <unistd.h>

#define READ_SIZE sizeof(struct inotify_event) + NAME_MAX + 1

static bool _readCb(void* fileWatcher);

typedef struct
{
    char const* fileName;
    FWCallback callback;
} Watch;

FileWatcher* FileWatcher_Create(void)
{
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0)
    {
        LISSANDRA_LOG_SYSERROR("inotify_init1");
        return NULL;
    }

    FileWatcher* fw = Malloc(sizeof(FileWatcher));
    fw->Handle = fd;
    fw->ReadCallback = _readCb;
    fw->_destroy = FileWatcher_Destroy;

    fw->WatchCallbacks = hashmap_create();
    return fw;
}

bool FileWatcher_AddWatch(FileWatcher* fw, char const* pathName, FWCallback notifyFn)
{
    int wd = inotify_add_watch(fw->Handle, pathName, IN_CLOSE_WRITE);
    if (wd < 0)
    {
        LISSANDRA_LOG_SYSERROR("inotify_add_watch");
        return false;
    }

    Watch* watch = Malloc(sizeof(Watch));
    watch->fileName = pathName;
    watch->callback = notifyFn;

    hashmap_put(fw->WatchCallbacks, wd, watch);
    return true;
}

void FileWatcher_Destroy(void* elem)
{
    FileWatcher* fw = (FileWatcher*) elem;
    hashmap_destroy_and_destroy_elements(fw->WatchCallbacks, Free);
    close(fw->Handle);
    Free(fw);
}

static ssize_t _readDesc(int fd, void* buf)
{
    /* Al parecer el descriptor de inotify solo envía structs enteras
     * En el caso de que se modifiquen varios archivos, y uno de ellos tenga el nombre muy largo
     * se atiende correctamente.
     */

    ssize_t readBytes = read(fd, buf, READ_SIZE);
    if (readBytes < 0)
    {
        // try again next loop
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
            LISSANDRA_LOG_SYSERROR("read");
        return -1;
    }

    return readBytes;
}

static bool _readCb(void* fileWatcher)
{
    FileWatcher* fw = (FileWatcher*) fileWatcher;

    uint8_t buf[READ_SIZE];
    uint8_t* src = buf;
    for (ssize_t readBytes = _readDesc(fw->Handle, buf); readBytes > 0; readBytes += _readDesc(fw->Handle, buf), src = buf)
    {
        while (readBytes > 0)
        {
            size_t const chunkSize = sizeof(struct inotify_event);

            struct inotify_event event;
            char fileName[NAME_MAX + 1];
            memcpy(&event, src, chunkSize);

            readBytes -= chunkSize;
            src += chunkSize;

            if (event.len > 0)
            {
                memcpy(fileName, src, event.len);
                readBytes -= event.len;
                src += event.len;
            }

            Watch* watch = (Watch*) hashmap_get(fw->WatchCallbacks, event.wd);
            if (!watch)
            {
                LISSANDRA_LOG_ERROR("FileWatcher callback: wd %u no registrado!", event.wd);
                continue;
            }

            watch->callback(event.len ? fileName : watch->fileName);
        }
    }

    return true;
}
