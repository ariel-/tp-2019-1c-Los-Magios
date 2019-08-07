
#ifndef Threads_h__
#define Threads_h__

#include "Logger.h"
#include <pthread.h>

typedef void* WorkerFn(void*);

// inicia un nuevo thread como detached
static inline bool Threads_CreateDetached(WorkerFn* fn, void* param)
{
    pthread_attr_t attr;
    int r = pthread_attr_init(&attr);
    if (r < 0)
    {
        LISSANDRA_LOG_SYSERR(r, "pthread_attr_init");
        return false;
    }

    r = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (r < 0)
    {
        LISSANDRA_LOG_SYSERR(r, "pthread_attr_setdetachstate");
        return false;
    }

    pthread_t tid;
    r = pthread_create(&tid, &attr, fn, param);
    if (r < 0)
    {
        LISSANDRA_LOG_SYSERR(r, "pthread_create");
        return false;
    }

    r = pthread_attr_destroy(&attr);
    if (r < 0)
    {
        LISSANDRA_LOG_SYSERR(r, "pthread_attr_destroy");
        return false;
    }

    return true;
}

#endif //Threads_h__
