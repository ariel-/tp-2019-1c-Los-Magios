
#ifndef Config_h__
#define Config_h__

#include <Defines.h>
#include <linux/limits.h>
#include <netinet/in.h>
#include <stdint.h>
#include <vector.h>

typedef struct
{
    char PUERTO[PORT_STRLEN];
    char IP_FS[INET6_ADDRSTRLEN];
    char PUERTO_FS[PORT_STRLEN];
    Vector IP_SEEDS;
    Vector PUERTO_SEEDS;
    size_t TAM_MEM;
    uint32_t MEMORY_NUMBER;

    // Campos recargables en runtime
    uint32_t RETARDO_MEM;
    uint32_t RETARDO_FS;
    uint32_t RETARDO_JOURNAL;
    uint32_t RETARDO_GOSSIPING;
} MemConfig;

extern MemConfig ConfigMemoria;

#endif //Config_h__
