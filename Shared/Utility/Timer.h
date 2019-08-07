
#ifndef Timer_h__
#define Timer_h__

#include "FileDescInterface.h"
#include <stdint.h>
#include <time.h>

typedef struct PeriodicTimer PeriodicTimer;

typedef void TimerCallbackFnType(PeriodicTimer*);
typedef struct PeriodicTimer
{
    FDI _impl;

    struct timespec Interval;
    TimerCallbackFnType* Callback;
    bool TimerEnabled;
} PeriodicTimer;

// Crea un nuevo timer: intervalo en milisegundos, y funcion de llamada cuando el mismo expire
PeriodicTimer* PeriodicTimer_Create(uint32_t intervalMS, TimerCallbackFnType* callback);

// activa o desactiva el timer
void PeriodicTimer_SetEnabled(PeriodicTimer* pt, bool enable);

// cambia el intervalo de tiempo
void PeriodicTimer_ReSetTimer(PeriodicTimer* pt, uint32_t newIntervalMS);

// destruye un periodic timer
void PeriodicTimer_Destroy(void* timer);

static inline uint64_t TimeSpecToMS(struct timespec const* ts)
{
    // nano  = 10 ^ (-9)
    // milli = 10 ^ (-3)
    // divide by 10 ^ 6 to get milli
    return ts->tv_sec * 1000ULL + ts->tv_nsec / 1000000ULL;
}

static inline struct timespec MSToTimeSpec(uint32_t ms)
{
    return (struct timespec)
    {
        .tv_sec  = ms / 1000U,
        .tv_nsec = (ms % 1000U) * 1000000U
    };
}

static inline uint64_t GetMSTimeDiff(uint64_t oldMSTime, uint64_t newMSTime)
{
    return newMSTime - oldMSTime;
}

// returns current tick in milliseconds
static inline uint64_t GetMSEpoch(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return TimeSpecToMS(&ts);
}

static inline uint64_t GetMSTime(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return TimeSpecToMS(&ts);
}

// sleep for some ms
static inline void MSSleep(uint32_t ms)
{
    struct timespec const ts = MSToTimeSpec(ms);
    nanosleep(&ts, NULL);
}

#endif //Timer_h__
