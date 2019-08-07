
#include "Timer.h"
#include "FDIImpl.h"
#include "Logger.h"
#include "Malloc.h"
#include <sys/timerfd.h>
#include <unistd.h>

static bool _readCb(void* periodicTimer);

PeriodicTimer* PeriodicTimer_Create(uint32_t intervalMS, TimerCallbackFnType* callback)
{
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (fd < 0)
    {
        LISSANDRA_LOG_SYSERROR("timerfd_create");
        return NULL;
    }

    struct timespec const interval = MSToTimeSpec(intervalMS);
    struct itimerspec its =
    {
        .it_interval = interval,
        .it_value = interval
    };

    if (timerfd_settime(fd, 0, &its, NULL) < 0)
    {
        LISSANDRA_LOG_SYSERROR("timerfd_settime");
        return NULL;
    }

    PeriodicTimer* pt = Malloc(sizeof(PeriodicTimer));
    pt->Handle = fd;
    pt->ReadCallback = _readCb;
    pt->_destroy = PeriodicTimer_Destroy;

    pt->Interval = interval;
    pt->Callback = callback;
    pt->TimerEnabled = true;
    return pt;
}

void PeriodicTimer_SetEnabled(PeriodicTimer* pt, bool enable)
{
    if (pt->TimerEnabled == enable)
        return;

    if (!enable)
    {
        // disarm timer
        struct itimerspec its = { 0 };
        if (timerfd_settime(pt->Handle, 0, &its, NULL) < 0)
            LISSANDRA_LOG_SYSERROR("timerfd_settime");
    }
    else
    {
        // rearm timer with old interval
        struct itimerspec its =
        {
            .it_interval = pt->Interval,
            .it_value = pt->Interval
        };
        if (timerfd_settime(pt->Handle, 0, &its, NULL))
            LISSANDRA_LOG_SYSERROR("timerfd_settime");
    }

    pt->TimerEnabled = enable;
}

void PeriodicTimer_ReSetTimer(PeriodicTimer* pt, uint32_t newIntervalMS)
{
    struct timespec const newInterval = MSToTimeSpec(newIntervalMS);

    // si el nuevo intervalo es igual al que ya tengo salgo en lugar de hacer syscalls
    if (!memcmp(&pt->Interval, &newInterval, sizeof(struct timespec)))
        return;

    // disable timer
    PeriodicTimer_SetEnabled(pt, false);

    // enable timer with new interval
    pt->Interval = newInterval;
    PeriodicTimer_SetEnabled(pt, true);
}

void PeriodicTimer_Destroy(void* timer)
{
    PeriodicTimer* pt = timer;
    close(pt->Handle);
    Free(pt);
}

static bool _readCb(void* periodicTimer)
{
    PeriodicTimer* pt = periodicTimer;
    uint64_t expiries;
    if (read(pt->Handle, &expiries, sizeof(uint64_t)) < 0)
    {
        // try again next time
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
            LISSANDRA_LOG_SYSERROR("read");
        return true;
    }

    for (uint64_t i = 0; pt->TimerEnabled && i < expiries; ++i)
        pt->Callback(pt);
    return true;
}
