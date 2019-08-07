
#include "Metric.h"
#include <Logger.h>
#include <Malloc.h>
#include <Timer.h>

typedef struct Metric
{
    MetricEvent Type;
    uint64_t Timestamp;

    // para los tiempos de exec
    uint64_t Value;

    struct Metric* Next;
} Metric;

typedef struct Metrics
{
    Metric* Head;
    Metric* Tail;
} Metrics;

static void _pruneOldEvents(Metrics*);

Metrics* Metrics_Create(void)
{
    Metrics* m = Malloc(sizeof(Metrics));
    m->Head = NULL;
    m->Tail = NULL;
    return m;
}

void Metrics_Add(Metrics* m, MetricEvent event, uint64_t value)
{
    Metric* metric = Malloc(sizeof(Metric));
    metric->Type = event;
    metric->Timestamp = GetMSTime();
    metric->Value = value;
    metric->Next = NULL;

    if (!m->Head)
    {
        m->Head = metric;
        m->Tail = metric;
    }
    else
    {
        m->Tail->Next = metric;
        m->Tail = metric;
    }
}

void Metrics_Report(Metrics* m, LogLevel logLevel)
{
    // limpiar eventos viejos
    _pruneOldEvents(m);

    uint64_t readLatencySum = 0;
    uint64_t selectCount = 0;

    uint64_t writeLatencySum = 0;
    uint64_t insertCount = 0;

    for (Metric* i = m->Head; i != NULL; i = i->Next)
    {
        switch (i->Type)
        {
            case EVT_READ_LATENCY:
                readLatencySum += i->Value;
                ++selectCount;
                break;
            case EVT_WRITE_LATENCY:
                writeLatencySum += i->Value;
                ++insertCount;
                break;
            default:
                continue;
        }
    }

    // tiempo promedio que tarda un SELECT en los ultimos 30 seg
    double readLatencyMean = 0.0;
    if (selectCount)
        readLatencyMean = readLatencySum / (double) selectCount;

    // tiempo promedio que tarda un INSERT en los ultimos 30 seg
    double writeLatencyMean = 0.0;
    if (insertCount)
        writeLatencyMean = writeLatencySum / (double) insertCount;

    LISSANDRA_LOG_MESSAGE(logLevel, "Latencia promedio SELECT/30s: %.0fms", readLatencyMean);
    LISSANDRA_LOG_MESSAGE(logLevel, "Latencia promedio INSERT/30s: %.0fms", writeLatencyMean);
    LISSANDRA_LOG_MESSAGE(logLevel, "Cantidad SELECT/30s: %" PRIu64, selectCount);
    LISSANDRA_LOG_MESSAGE(logLevel, "Cantidad INSERT/30s: %" PRIu64, insertCount);
}

void Metrics_Destroy(Metrics* m)
{
    Metric* metric = m->Head;
    while (metric != NULL)
    {
        Metric* next = metric->Next;
        Free(metric);
        metric = next;
    }

    Free(m);
}

/* PRIVATE */
static void _pruneOldEvents(Metrics* m)
{
    // reportar eventos ocurridos en los ultimos 30 segundos inclusive
    static uint32_t const CUT_INTERVAL_MS = 30000U;

    // la lista está ordenada. el evento mas reciente es el ultimo
    // hay que considerar solo los eventos que ocurrieron hasta 30 segs antes, descartando los otros
    // por lo tanto el primer elemento que no cumpla es mi condicion de corte

    uint64_t now = GetMSTime();

    Metric* metric = m->Head;
    for (; metric != NULL; metric = metric->Next)
    {
        // busco el primer elemento mas nuevo y corto la iteracion
        if (metric->Timestamp + CUT_INTERVAL_MS >= now)
            break;
    }

    // eliminamos los elementos restantes (mas viejos que 30 seg)
    for (Metric* i = m->Head; i != metric;)
    {
        Metric* next = i->Next;
        Free(i);
        i = next;
    }

    m->Head = metric;
}
