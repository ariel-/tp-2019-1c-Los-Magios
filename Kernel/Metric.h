
#ifndef Metric_h__
#define Metric_h__

#include <LogCommon.h>
#include <stdint.h>

typedef enum
{
    EVT_READ_LATENCY,  // loguea un tiempo de ejecucion SELECT
    EVT_WRITE_LATENCY  // loguea un tiempo de ejecucion INSERT
} MetricEvent;

typedef struct Metrics Metrics;

Metrics* Metrics_Create(void);

void Metrics_Add(Metrics*, MetricEvent, uint64_t);

void Metrics_Report(Metrics*, LogLevel);

void Metrics_Destroy(Metrics*);

#endif //Metric_h__
