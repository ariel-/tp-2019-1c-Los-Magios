
#ifndef Config_h__
#define Config_h__

#include <Defines.h>
#include <linux/limits.h>
#include <netinet/in.h>
#include <stdint.h>

typedef struct
{
    char IP_MEMORIA[INET6_ADDRSTRLEN];
    char PUERTO_MEMORIA[PORT_STRLEN];
    uint32_t MULTIPROCESAMIENTO;

    // Campos recargables en runtime
    uint32_t METADATA_REFRESH;
    _Atomic uint32_t SLEEP_EJECUCION;
    _Atomic uint32_t QUANTUM;
} KernelConfig;

extern KernelConfig ConfigKernel;

#endif //Config_h__
