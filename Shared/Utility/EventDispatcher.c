
#include "EventDispatcher.h"
#include "Console.h"
#include "FileDescInterface.h"
#include "Logger.h"
#include "Malloc.h"
#include "Timer.h"
#include "vector.h"
#include <assert.h>
#include <libcommons/hashmap.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define PER_LOOP_FDS 20

typedef struct EventDispatcher
{
    int Handle;

    struct epoll_event _events[PER_LOOP_FDS];
    t_hashmap* _fdiMap;
} EventDispatcher;

static EventDispatcher sDispatcher;

static void _dispatchEvent(struct epoll_event* evt);

bool EventDispatcher_Init(void)
{
    sDispatcher.Handle = epoll_create1(0);
    if (sDispatcher.Handle < 0)
    {
        LISSANDRA_LOG_SYSERROR("epoll_create1");
        return false;
    }

    memset(sDispatcher._events, 0, sizeof(struct epoll_event) * PER_LOOP_FDS);
    sDispatcher._fdiMap = hashmap_create();
    return true;
}

void EventDispatcher_AddFDI(void* interface)
{
    FDI* fdi = interface;

    struct epoll_event evt =
    {
        .events = EPOLLIN,
        .data = { .ptr = interface }
    };
    if (epoll_ctl(sDispatcher.Handle, EPOLL_CTL_ADD, fdi->Handle, &evt) < 0)
        LISSANDRA_LOG_SYSERROR("epoll_ctl ADD");
    hashmap_put(sDispatcher._fdiMap, fdi->Handle, fdi);
}

void EventDispatcher_RemoveFDI(void* interface)
{
    FDI* fdi = interface;

    static struct epoll_event dummy;
    if (epoll_ctl(sDispatcher.Handle, EPOLL_CTL_DEL, fdi->Handle, &dummy) < 0)
        LISSANDRA_LOG_SYSERROR("epoll_ctl DEL");
    hashmap_remove_and_destroy(sDispatcher._fdiMap, fdi->Handle, FDI_Destroy);
}

void EventDispatcher_Dispatch(void)
{
    int res = epoll_wait(sDispatcher.Handle, sDispatcher._events, PER_LOOP_FDS, -1);
    if (res < 0)
    {
        if (errno != EINTR)
            LISSANDRA_LOG_SYSERROR("epoll_wait");
        return;
    }

    for (int i = 0; i < res; ++i)
        _dispatchEvent(sDispatcher._events + i);
}

void EventDispatcher_Loop(void)
{
    while (ProcessRunning)
    {
        // procesar fds (sockets, inotify, etc...)
        EventDispatcher_Dispatch();
    }
}

void EventDispatcher_Terminate(void)
{
    hashmap_destroy_and_destroy_elements(sDispatcher._fdiMap, FDI_Destroy);
    close(sDispatcher.Handle);
}

static void _dispatchEvent(struct epoll_event* evt)
{
    FDI* fdi = evt->data.ptr;
    if (evt->events & (EPOLLERR | EPOLLHUP))
    {
        LISSANDRA_LOG_TRACE("Desconectando fd %u no válido o desconectado", fdi->Handle);
        EventDispatcher_RemoveFDI(fdi);
        return;
    }

    // chequear el valor de retorno:
    // si el callback devuelve false, tengo que quitarlo de la lista de watch de epoll
    // ya que el file descriptor se ha cerrado
    if (evt->events & EPOLLIN)
    {
        if (fdi->ReadCallback && !fdi->ReadCallback(fdi))
        {
            EventDispatcher_RemoveFDI(fdi);
            return;
        }
    }
}
