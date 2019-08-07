
#include "Criteria.h"
#include "Config.h"
#include "PacketBuilders.h"
#include <Defines.h>
#include <libcommons/hashmap.h>
#include <libcommons/list.h>
#include <Logger.h>
#include <Malloc.h>
#include <pthread.h>
#include <Socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <Timer.h>
#include <vector.h>

typedef struct Memory
{
    uint32_t MemId;
    uint64_t MemoryOperations;
    Socket* MemSocket;
    pthread_mutex_t MemLock;
} Memory;

typedef struct
{
    uint32_t MemId;
    uint64_t MemOperations;
} MemoryStats;

// feo copypaste de Memoria/Gossip.c pero we
typedef struct
{
    char IP[INET6_ADDRSTRLEN];
    char Port[PORT_STRLEN];
} MemoryConnectionData;

static inline void _addToIPPortHmap(t_hashmap* hmap, uint32_t memId, char const* IP, char const* Port)
{
    MemoryConnectionData* mcd = hashmap_get(hmap, memId);
    if (!mcd)
    {
        mcd = Malloc(sizeof(MemoryConnectionData));
        hashmap_put(hmap, memId, mcd);
    }

    snprintf(mcd->IP, INET6_ADDRSTRLEN, "%s", IP);
    snprintf(mcd->Port, PORT_STRLEN, "%s", Port);
}

static void _iteratePeer(int, void*, void*);
static void _disconnectOldMemories(int, void*, void*);
static void _logNewMemories(int, void*);

static void _disconnectMemory(uint32_t memId);

typedef void AddMemoryFnType(Memory* mem);
typedef Memory* GetMemFnType(MemoryOps op, DBRequest const* dbr);
typedef void MemoryCountOperationsFnType(t_list*);
typedef void DestroyFnType(void);

typedef struct
{
    Metrics* CritMetrics;
    pthread_mutex_t CritLock;

    AddMemoryFnType* AddMemoryFn;
    GetMemFnType* GetMemFn;
    MemoryCountOperationsFnType* CountFn;
    DestroyFnType* DestroyFn;
} Criteria;

static void _destroy_mem(void* elem);
static void _destroy(Criteria* criteria);

static AddMemoryFnType _sc_add;
static AddMemoryFnType _shc_add;
static AddMemoryFnType _ec_add;

static GetMemFnType _sc_get;
static GetMemFnType _shc_get;
static GetMemFnType _ec_get;

static MemoryCountOperationsFnType _sc_count;
static MemoryCountOperationsFnType _shc_count;
static MemoryCountOperationsFnType _ec_count;

static DestroyFnType _sc_destroy;
static DestroyFnType _shc_destroy;
static DestroyFnType _ec_destroy;

static struct
{
    Criteria _impl;

    // SC: Maneja una sola memoria
    Memory SCMem;
} Criteria_SC = { { NULL, PTHREAD_MUTEX_INITIALIZER, _sc_add, _sc_get, _sc_count, _sc_destroy }, { 0 } };

static struct
{
    Criteria _impl;

    Vector MemoryArr;
} Criteria_SHC = { { NULL, PTHREAD_MUTEX_INITIALIZER, _shc_add, _shc_get, _shc_count, _shc_destroy }, { 0 } };

static struct
{
    Criteria _impl;

    Vector MemoryArr;
} Criteria_EC = { { NULL, PTHREAD_MUTEX_INITIALIZER, _ec_add, _ec_get, _ec_count, _ec_destroy }, { 0 } };

static void _send_one(Memory* mem, MemoryOps op, DBRequest const* dbr);

static Criteria* Criterias[NUM_CRITERIA] = { &Criteria_SC._impl, &Criteria_SHC._impl, &Criteria_EC._impl };
static t_hashmap* MemoryIPMap = NULL;

static inline uint16_t keyHash(uint16_t key)
{
    uint16_t hash = 0x5BA9U;
    uint8_t const* k = (uint8_t*) &key;
    for (uint8_t i = 0; i < sizeof(uint16_t); ++i)
    {
        hash ^= (uint16_t) k[i];
        hash *= 0x0287;
    }
    return hash;
}
void Criterias_Init(PeriodicTimer* discoverTimer)
{
    MemoryIPMap = hashmap_create();

    srandom(GetMSTime());

    // general
    for (uint8_t i = 0; i < NUM_CRITERIA; ++i)
        Criterias[i]->CritMetrics = Metrics_Create();

    // SHC
    Vector_Construct(&Criteria_SHC.MemoryArr, sizeof(Memory), _destroy_mem, 0);

    // EC
    Vector_Construct(&Criteria_EC.MemoryArr, sizeof(Memory), _destroy_mem, 0);

    Criterias_Update(discoverTimer);
}

void Criterias_Update(PeriodicTimer* pt)
{
    PeriodicTimer_SetEnabled(pt, false);

    {
        // si la seed no esta porque se desconecto o es el gossip inicial o whatever, la agregamos a manopla
        if (!hashmap_has_key(MemoryIPMap, 0))
            _addToIPPortHmap(MemoryIPMap, 0, ConfigKernel.IP_MEMORIA, ConfigKernel.PUERTO_MEMORIA);
    }

    t_hashmap* diff = hashmap_create();

    // descubre nuevas memorias y las agrego a diff
    hashmap_iterate_with_data(MemoryIPMap, _iteratePeer, diff);

    // quita las que tengo y no aparecen en el nuevo diff
    hashmap_iterate_with_data(MemoryIPMap, _disconnectOldMemories, diff);

    // logueo nuevos discoveries, se agregan cuando cambio el mapa por el temporal
    hashmap_iterate(diff, _logNewMemories);

    // por ultimo el diccionario queda actualizado con los nuevos items
    hashmap_destroy_and_destroy_elements(MemoryIPMap, Free);
    MemoryIPMap = diff;

    PeriodicTimer_SetEnabled(pt, true);
}

bool Criteria_MemoryExists(uint32_t memId)
{
    return hashmap_has_key(MemoryIPMap, memId);
}

void Criteria_AddMemory(CriteriaType type, uint32_t memId)
{
    MemoryConnectionData* mcd = hashmap_get(MemoryIPMap, memId);
    if (!mcd)
    {
        LISSANDRA_LOG_ERROR("Criteria_AddMemory: Intento de agregar memoria %u no existente", memId);
        return;
    }

    SocketOpts const so =
    {
        .HostName = mcd->IP,
        .ServiceOrPort = mcd->Port,
        .SocketMode = SOCKET_CLIENT,
        .SocketOnAcceptClient = NULL
    };
    Socket* s = Socket_Create(&so);
    if (!s)
    {
        LISSANDRA_LOG_ERROR("Criteria_AddMemory: Memoria %u desconectada!", memId);
        _disconnectMemory(memId);
        hashmap_remove_and_destroy(MemoryIPMap, memId, Free);
        return;
    }

    Memory mem;
    mem.MemId = memId;
    mem.MemoryOperations = 0;
    mem.MemSocket = s;
    pthread_mutex_init(&mem.MemLock, NULL);

    Criterias[type]->AddMemoryFn(&mem);
}

void Criteria_AddMetric(CriteriaType type, MetricEvent event, uint64_t value)
{
    Criteria* const itr = Criterias[type];

    pthread_mutex_lock(&itr->CritLock);
    Metrics_Add(itr->CritMetrics, event, value);
    pthread_mutex_unlock(&itr->CritLock);
}

static bool _sortByMemIdPred(void* left, void* right)
{
    MemoryStats* const a = left;
    MemoryStats* const b = right;

    return a->MemId < b->MemId;
}

static void* _sumOperations(void* partial, void* elem)
{
    uint64_t* const partialSum = partial;
    MemoryStats* const stats = elem;

    *partialSum += stats->MemOperations;
    return partialSum;
}

static void _report_one(MemoryStats const* stats, uint64_t total, LogLevel logLevel)
{
    double load = 0.0;
    if (total)
        load = stats->MemOperations / (double) total;

    static char const* fullbar = "##################################################";
    uint32_t const prog_width = 30;

    uint32_t const prog_print = (uint32_t) (load * prog_width + 0.5);
    uint32_t const rest_print = prog_width - prog_print;

    LISSANDRA_LOG_MESSAGE(logLevel, "Carga de memoria %u: %6.2f%% [%.*s%*s]", stats->MemId, load * 100.0, prog_print, fullbar, rest_print, "");
}

void Criterias_Report(PeriodicTimer* pt)
{
    // si es por consola, logueo en consola las estadisticas
    LogLevel const logLevel = pt ? LOG_LEVEL_TRACE : LOG_LEVEL_INFO;

    static char const* const CRITERIA_NAMES[NUM_CRITERIA] =
    {
        "STRONG",
        "STRONG HASH",
        "EVENTUAL"
    };

    LISSANDRA_LOG_MESSAGE(logLevel, "========REPORTE========");
    // para memory load
    t_list* memStatisticList = list_create();

    for (unsigned type = 0; type < NUM_CRITERIA; ++type)
    {
        LISSANDRA_LOG_MESSAGE(logLevel, "CRITERIO %s", CRITERIA_NAMES[type]);

        Criteria* const itr = Criterias[type];
        pthread_mutex_lock(&itr->CritLock);

        Metrics_Report(itr->CritMetrics, logLevel);

        // contabilizar cantidad de operaciones por memoria
        itr->CountFn(memStatisticList);

        pthread_mutex_unlock(&itr->CritLock);

        LISSANDRA_LOG_MESSAGE(logLevel, "\n");
    }

    // ordenamos la lista por memId
    list_sort(memStatisticList, _sortByMemIdPred);

    // recorrer la lista para conocer el total de operaciones
    uint64_t totalOps = 0;
    list_reduce(memStatisticList, &totalOps, _sumOperations);

    // imprimir carga de memoria (operaciones mem/total)
    for (t_link_element* itr = memStatisticList->head; itr != NULL; itr = itr->next)
        _report_one(itr->data, totalOps, logLevel);

    // liberar memoria
    list_destroy_and_destroy_elements(memStatisticList, Free);

    LISSANDRA_LOG_MESSAGE(logLevel, "======FIN REPORTE======");
}

static void _journalize(void* mem)
{
    _send_one(mem, OP_JOURNAL, NULL);
}

void Criterias_BroadcastJournal(void)
{
    if (Criteria_SC.SCMem.MemId)
        _journalize(&Criteria_SC.SCMem);

    Vector_iterate(&Criteria_SHC.MemoryArr, _journalize);

    Vector_iterate(&Criteria_EC.MemoryArr, _journalize);
}

Memory* Criteria_GetMemoryFor(CriteriaType type, MemoryOps op, DBRequest const* dbr)
{
    // elegir cualquier criterio que tenga memorias conectadas
    if (type == CRITERIA_ANY)
    {
        CriteriaType types[NUM_CRITERIA] = { 0 };
        uint32_t last = 0;

        if (Criteria_SC.SCMem.MemId)
            types[last++] = CRITERIA_SC;

        if (!Vector_empty(&Criteria_SHC.MemoryArr))
            types[last++] = CRITERIA_SHC;

        if (!Vector_empty(&Criteria_EC.MemoryArr))
            types[last++] = CRITERIA_EC;

        if (!last)
        {
            LISSANDRA_LOG_ERROR("Criteria: no hay memorias conectadas. Utilizar ADD primero!");
            return NULL;
        }

        type = types[random() % last];
    }

    return Criterias[type]->GetMemFn(op, dbr);
}

void Memory_SendRequest(Memory* mem, MemoryOps op, DBRequest const* dbr)
{
    pthread_mutex_lock(&mem->MemLock);
    _send_one(mem, op, dbr);
    pthread_mutex_unlock(&mem->MemLock);
}

Packet* Memory_SendRequestWithAnswer(Memory* mem, MemoryOps op, DBRequest const* dbr)
{
    pthread_mutex_lock(&mem->MemLock);
    _send_one(mem, op, dbr);
    Packet* p = Socket_RecvPacket(mem->MemSocket);
    pthread_mutex_unlock(&mem->MemLock);

    if (!p)
    {
        uint32_t memId = mem->MemId;

        LISSANDRA_LOG_ERROR("Se desconectó memoria %u mientras leia un request!!, quitando...", memId);
        _disconnectMemory(memId);
        hashmap_remove_and_destroy(MemoryIPMap, memId, Free);
    }

    return p;
}

void Criterias_Destroy(void)
{
    for (unsigned type = 0; type < NUM_CRITERIA; ++type)
        _destroy(Criterias[type]);

    hashmap_destroy_and_destroy_elements(MemoryIPMap, Free);
}

/* PRIVATE */
static void _iteratePeer(int _, void* val, void* diff)
{
    (void) _;
    MemoryConnectionData* const mcd = val;

    SocketOpts const so =
    {
        .HostName = mcd->IP,
        .ServiceOrPort = mcd->Port,
        .SocketMode = SOCKET_CLIENT,
        .SocketOnAcceptClient = NULL
    };
    Socket* s = Socket_Create(&so);
    if (!s)
    {
        LISSANDRA_LOG_ERROR("DISCOVER: No pude conectarme al par gossip (%s:%s)!!", mcd->IP, mcd->Port);
        return;
    }

    static uint8_t const id = KERNEL;
    Packet* p = Packet_Create(MSG_HANDSHAKE, 20);
    Packet_Append(p, id);
    Packet_Append(p, mcd->IP);
    Socket_SendPacket(s, p);
    Packet_Destroy(p);

    p = Socket_RecvPacket(s);
    if (!p)
    {
        LISSANDRA_LOG_ERROR("DISCOVER: Memoria (%s:%s) se desconectó durante handshake inicial!", mcd->IP, mcd->Port);
        Socket_Destroy(s);
        return;
    }

    if (Packet_GetOpcode(p) != MSG_GOSSIP_LIST)
    {
        LISSANDRA_LOG_ERROR("DISCOVER: Memoria (%s:%s) envió respuesta inválida %hu.", mcd->IP, mcd->Port, Packet_GetOpcode(p));
        Packet_Destroy(p);
        Socket_Destroy(s);
        return;
    }

    uint32_t numMems;
    Packet_Read(p, &numMems);
    for (uint32_t i = 0; i < numMems; ++i)
    {
        uint32_t memId;
        Packet_Read(p, &memId);

        char* memIp;
        Packet_Read(p, &memIp);

        char* memPort;
        Packet_Read(p, &memPort);

        _addToIPPortHmap(diff, memId, memIp, memPort);

        Free(memPort);
        Free(memIp);
    }

    Packet_Destroy(p);
    Socket_Destroy(s);
}

static void _logNewMemories(int key, void* val)
{
    MemoryConnectionData* const mcd = val;

    MemoryConnectionData* my = hashmap_get(MemoryIPMap, key);
    if (my)
        return;

    LISSANDRA_LOG_INFO("DISCOVER: Descubierta nueva memoria en %s:%s! (MemId: %u)", mcd->IP, mcd->Port, (uint32_t) key);
}

static void _disconnectOldMemories(int key, void* val, void* diff)
{
    MemoryConnectionData* const mcd = val;
    if (key /* la seed no cuenta */ && !hashmap_has_key(diff, key))
    {
        LISSANDRA_LOG_INFO("DISCOVER: Memoria id %u (%s:%s) se desconecto!", (uint32_t) key, mcd->IP, mcd->Port);
        _disconnectMemory(key);
    }
}

static void _removeFromArray(Vector* array, uint32_t memId)
{
    Memory* const data = Vector_data(array);
    for (size_t i = 0; i < Vector_size(array); ++i)
    {
        if (data[i].MemId == memId)
        {
            Vector_erase(array, i);
            break;
        }
    }
}

static void _disconnectMemory(uint32_t memId)
{
    if (!Criteria_MemoryExists(memId))
    {
        LISSANDRA_LOG_ERROR("Intento de quitar memoria %u no existente!", memId);
        return;
    }

    if (Criteria_SC.SCMem.MemId == memId)
        _destroy_mem(&Criteria_SC.SCMem);

    _removeFromArray(&Criteria_SHC.MemoryArr, memId);

    _removeFromArray(&Criteria_EC.MemoryArr, memId);
}

static void _destroy_mem(void* elem)
{
    Memory* const m = elem;
    m->MemId = 0;
    pthread_mutex_destroy(&m->MemLock);
    Socket_Destroy(m->MemSocket);
}

static void _destroy(Criteria* c)
{
    Metrics_Destroy(c->CritMetrics);
    c->DestroyFn();
}

static void _sc_add(Memory* mem)
{
    if (Criteria_SC.SCMem.MemId)
    {
        LISSANDRA_LOG_ERROR("El criterio SC ya tiene una memoria asignada! (memId: %u)", Criteria_SC.SCMem.MemId);
        _destroy_mem(mem);
        return;
    }

    Criteria_SC.SCMem = *mem;
}

static void _addMemoryToArray(Vector* array, Memory* mem, char const* criteria)
{
    Memory* const data = Vector_data(array);
    for (size_t i = 0; i < Vector_size(array); ++i)
    {
        if (data[i].MemId == mem->MemId)
        {
            LISSANDRA_LOG_ERROR("El criterio %s ya tiene asignada la memoria %u!", criteria, mem->MemId);
            _destroy_mem(mem);
            return;
        }
    }

    Vector_push_back(array, mem);
}

static void _shc_add(Memory* mem)
{
    _addMemoryToArray(&Criteria_SHC.MemoryArr, mem, "SHC");
}

static void _ec_add(Memory* mem)
{
    _addMemoryToArray(&Criteria_EC.MemoryArr, mem, "EC");
}

static Memory* _sc_get(MemoryOps op, DBRequest const* dbr)
{
    (void) dbr;

    if (!Criteria_SC.SCMem.MemId)
    {
        LISSANDRA_LOG_ERROR("Criterio SC: sin memoria asociada! (op %d)", op);
        return NULL;
    }

    return &Criteria_SC.SCMem;
}

static Memory* _shc_get(MemoryOps op, DBRequest const* dbr)
{
    if (Vector_empty(&Criteria_SHC.MemoryArr))
    {
        LISSANDRA_LOG_ERROR("Criterio SHC: no hay memorias asociadas! (op %d)", op);
        return NULL;
    }

    // por defecto cualquier memoria de las que tenga el criterio (ver issue 1326)
    size_t hash = random();
    switch (op)
    {
        case OP_SELECT:
            hash = keyHash(dbr->Data.Select.Key);
            break;
        case OP_INSERT:
            hash = keyHash(dbr->Data.Insert.Key);
            break;
        default:
            break;
    }

    return Vector_at(&Criteria_SHC.MemoryArr, hash % Vector_size(&Criteria_SHC.MemoryArr));
}

static Memory* _ec_get(MemoryOps op, DBRequest const* dbr)
{
    (void) dbr;

    if (Vector_empty(&Criteria_EC.MemoryArr))
    {
        LISSANDRA_LOG_ERROR("Criterio EC: no hay memorias asociadas! (op %d)", op);
        return NULL;
    }

    // TODO: dejar random?
    // otras opciones:
    // LRU
    // RR
    // MR (min requests)
    return Vector_at(&Criteria_EC.MemoryArr, random() % Vector_size(&Criteria_EC.MemoryArr));
}

static bool _getStatByMemIdPred(void* elem, void* memId)
{
    MemoryStats* const stats = elem;
    return stats->MemId == *(uint32_t*) memId;
}

static inline void _addStatistic(t_list* memList, uint32_t memId, uint64_t operations)
{
    MemoryStats* stats = list_find(memList, _getStatByMemIdPred, &memId);
    if (!stats)
    {
        stats = Malloc(sizeof(MemoryStats));
        stats->MemId = memId;
        stats->MemOperations = 0;
        list_add(memList, stats);
    }

    stats->MemOperations += operations;
}

static void _sc_count(t_list* memList)
{
    Memory* const mem = &Criteria_SC.SCMem;
    if (!mem->MemId)
        return;

    pthread_mutex_lock(&mem->MemLock);
    _addStatistic(memList, mem->MemId, mem->MemoryOperations);
    pthread_mutex_unlock(&mem->MemLock);
}

static inline void _countAllInArray(Vector* array, t_list* memList)
{
    Memory* const data = Vector_data(array);

    for (size_t i = 0; i < Vector_size(array); ++i)
    {
        pthread_mutex_lock(&data[i].MemLock);
        _addStatistic(memList, data[i].MemId, data[i].MemoryOperations);
        pthread_mutex_unlock(&data[i].MemLock);
    }
}

static void _shc_count(t_list* memList)
{
    _countAllInArray(&Criteria_SHC.MemoryArr, memList);
}

static void _ec_count(t_list* memList)
{
    _countAllInArray(&Criteria_EC.MemoryArr, memList);
}

static void _sc_destroy(void)
{
    if (Criteria_SC.SCMem.MemId)
        _destroy_mem(&Criteria_SC.SCMem);
}

static void _shc_destroy(void)
{
    Vector_Destruct(&Criteria_SHC.MemoryArr);
}

static void _ec_destroy(void)
{
    Vector_Destruct(&Criteria_EC.MemoryArr);
}

static void _send_one(Memory* mem, MemoryOps op, DBRequest const* dbr)
{
    typedef Packet* PacketBuildFn(DBRequest const*);
    static PacketBuildFn* const PacketBuilders[NUM_OPS] =
    {
        BuildSelect,
        BuildInsert,
        BuildCreate,
        BuildDescribe,
        BuildDrop,
        BuildJournal
    };

    // metrica por memoria
    if (op == OP_SELECT || op == OP_INSERT)
        ++mem->MemoryOperations;

    Packet* p = PacketBuilders[op](dbr);
    Socket_SendPacket(mem->MemSocket, p);
    Packet_Destroy(p);
}
