
#include "Gossip.h"
#include "Config.h"
#include <Defines.h>
#include <libcommons/dictionary.h>
#include <libcommons/hashmap.h>
#include <Opcodes.h>
#include <Packet.h>
#include <Socket.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <Threads.h>
#include <Timer.h>

typedef struct
{
    uint32_t MemId;
    char IP[INET6_ADDRSTRLEN];
    char Port[PORT_STRLEN];
} GossipPeer;

// mi lista interna de seeds. una vez que una memoria se agrega nunca se quita
// mapa IP:PUERTO->GossipPeer
static t_dictionary* GossipPeers = NULL;

// la tabla que intercambiamos con otras memorias y el kernel
// mapa MemId->GossipPeer
static t_hashmap* GossipingTable = NULL;
// vamos a compartirla asi que necesito un mutex
static pthread_rwlock_t GossipTableLock = PTHREAD_RWLOCK_INITIALIZER;

// funcion para iterar la tabla y agregarlas al paquete
static void _addToPacket(int memId, void* elem, void* packet);

static void* _gossipWorkerTh(void*);

typedef struct
{
    t_dictionary* NewDiscoveries;
    t_hashmap* Blacklist;
} IterateParams;
static void _iterateSuccessfulMemories(char const* _, void* value, void* param);

static void _addToKnownPeers(char const* key, void* value);

// auxiliar agregar a diccionario
static inline void _addToIPPortDict(t_dictionary* dict, uint32_t memId, char const* IP, char const* Port)
{
    // suma + 1 de STR_LEN y + 1 de PORT_STRLEN pero está bien porque agrego el ':'
    char key[INET6_ADDRSTRLEN + PORT_STRLEN];
    snprintf(key, INET6_ADDRSTRLEN + PORT_STRLEN, "%s:%s", IP, Port);
    GossipPeer* gp = dictionary_get(dict, key);
    if (!gp)
        gp = Malloc(sizeof(GossipPeer));

    gp->MemId = memId;
    snprintf(gp->IP, INET6_ADDRSTRLEN, "%s", IP);
    snprintf(gp->Port, PORT_STRLEN, "%s", Port);

    dictionary_put(dict, key, gp);
}

static inline bool _IPPortDictHasKey(t_dictionary* dict, char const* IP, char const* Port)
{
    // suma + 1 de STR_LEN y + 1 de PORT_STRLEN pero está bien porque agrego el ':'
    char key[INET6_ADDRSTRLEN + PORT_STRLEN];
    snprintf(key, INET6_ADDRSTRLEN + PORT_STRLEN, "%s:%s", IP, Port);
    return dictionary_has_key(dict, key);
}

// same pero con el gossiplist, debe tener el lock antes de llamar
static inline void _addToGossipList(uint32_t memId, char const* IP, char const* Port)
{
    GossipPeer* gp = hashmap_get(GossipingTable, memId);
    if (!gp)
        gp = Malloc(sizeof(GossipPeer));

    gp->MemId = memId;
    snprintf(gp->IP, INET6_ADDRSTRLEN, "%s", IP);
    snprintf(gp->Port, PORT_STRLEN, "%s", Port);

    hashmap_put(GossipingTable, memId, gp);
}

// ultimo auxiliar para remover elementos de la gossiplist y blacklistearlos temporalmente
static inline void _removeAndBlock(t_hashmap* blacklist, uint32_t memId)
{
    pthread_rwlock_wrlock(&GossipTableLock);
    hashmap_remove_and_destroy(GossipingTable, memId, Free);
    pthread_rwlock_unlock(&GossipTableLock);

    hashmap_put(blacklist, memId, NULL);
}

void Gossip_Init(PeriodicTimer* gossipTimer)
{
    size_t ipSize = Vector_size(&ConfigMemoria.IP_SEEDS);
    if (ipSize != Vector_size(&ConfigMemoria.PUERTO_SEEDS))
    {
        LISSANDRA_LOG_FATAL("Error en archivo de configuración: no coinciden cantidad de ips y puertos!");
        exit(EXIT_FAILURE);
    }

    GossipPeers = dictionary_create();
    GossipingTable = hashmap_create();

    for (size_t i = 0; i < ipSize; ++i)
    {
        char** const ips = Vector_data(&ConfigMemoria.IP_SEEDS);
        char** const ports = Vector_data(&ConfigMemoria.PUERTO_SEEDS);

        // valor centinela: al inicio no conocemos que memId tienen los seeds
        _addToIPPortDict(GossipPeers, 0, ips[i], ports[i]);
    }

    Gossip_Do(gossipTimer);
}

void Gossip_AddMemory(uint32_t memId, char const* memIp, char const* memPort)
{
    pthread_rwlock_wrlock(&GossipTableLock);

    _addToGossipList(memId, memIp, memPort);

    pthread_rwlock_unlock(&GossipTableLock);
}

void Gossip_Do(PeriodicTimer* pt)
{
    PeriodicTimer_SetEnabled(pt, false);

    // llamar al worker thread, porque el connect bloquearia el main thread de otra forma
    Threads_CreateDetached(_gossipWorkerTh, pt);
}

void Gossip_SendTable(Socket* peer)
{
    Packet* p = Packet_Create(MSG_GOSSIP_LIST, 200);

    pthread_rwlock_rdlock(&GossipTableLock);

    uint32_t tableSize = hashmap_size(GossipingTable);
    Packet_Append(p, tableSize);
    hashmap_iterate_with_data(GossipingTable, _addToPacket, p);

    pthread_rwlock_unlock(&GossipTableLock);

    Socket_SendPacket(peer, p);
    Packet_Destroy(p);
}

void Gossip_Terminate(void)
{
    dictionary_destroy_and_destroy_elements(GossipPeers, Free);
    hashmap_destroy_and_destroy_elements(GossipingTable, Free);
}

// PRIVATE IMPLEMENTATION
static void _addToPacket(int memId, void* elem, void* packet)
{
    GossipPeer* gp = elem;
    Packet_Append(packet, (uint32_t) memId);
    Packet_Append(packet, gp->IP);
    Packet_Append(packet, gp->Port);
}

static void _iterateSuccessfulMemories(char const* _, void* value, void* param)
{
    (void) _;

    // evito auto-hacerme gossip (voy a estar presente ya que estoy en la tabla distribuida)
    GossipPeer* const gp = value;
    if (gp->MemId == ConfigMemoria.MEMORY_NUMBER)
        return;

    IterateParams* const storages = param;

    SocketOpts const so =
    {
        .HostName = gp->IP,
        .ServiceOrPort = gp->Port,
        .SocketMode = SOCKET_CLIENT,
        .SocketOnAcceptClient = NULL
    };
    Socket* s = Socket_Create(&so);
    if (!s)
    {
        LISSANDRA_LOG_DEBUG("GOSSIP: Memoria %s:%s no responde", gp->IP, gp->Port);

        // si no es la primer conexion conozco el mem id, borrar de tabla de gossip para mantenerla actualizada
        // y agrego a la blacklist para evitar que me pasen la memoria via gossip en esta iteracion
        if (gp->MemId)
            _removeAndBlock(storages->Blacklist, gp->MemId);
        return;
    }

    // paso 1: identificarme, soy memoria
    // paso mi id de memoria, mi puerto de escucha y el ip del peer (esto es porque el otro peer no conoce su ip)
    // yo tampoco conozco la mia por eso no la paso
    Packet* p = Packet_Create(MSG_HANDSHAKE, 200);

    static uint8_t const processId = MEMORIA;
    Packet_Append(p, processId);

    char* const otherIp = gp->IP;
    Packet_Append(p, otherIp);

    Packet_Append(p, ConfigMemoria.MEMORY_NUMBER);
    Packet_Append(p, ConfigMemoria.PUERTO);

    {
        pthread_rwlock_rdlock(&GossipTableLock);

        uint32_t numItems = hashmap_size(GossipingTable);
        Packet_Append(p, numItems);
        hashmap_iterate_with_data(GossipingTable, _addToPacket, p);

        pthread_rwlock_unlock(&GossipTableLock);
    }

    Socket_SendPacket(s, p);
    Packet_Destroy(p);

    // paso 2: recibir lista de gossip y mergear datos
    p = Socket_RecvPacket(s);
    if (!p)
    {
        LISSANDRA_LOG_DEBUG("GOSSIP: Memoria seed (%s:%s) se desconectó durante gossip!", gp->IP, gp->Port);
        // mismo de arriba
        if (gp->MemId)
            _removeAndBlock(storages->Blacklist, gp->MemId);

        Socket_Destroy(s);
        return;
    }

    if (Packet_GetOpcode(p) != MSG_GOSSIP_LIST)
    {
        LISSANDRA_LOG_TRACE("GOSSIP: recibi mensaje invalido %hu de %s:%s!", Packet_GetOpcode(p), gp->IP, gp->Port);
        Packet_Destroy(p);
        Socket_Destroy(s);
        return;
    }

    uint32_t num;
    Packet_Read(p, &num);
    for (uint32_t i = 0; i < num; ++i)
    {
        uint32_t elemMemId;
        Packet_Read(p, &elemMemId);

        char* elemIP;
        Packet_Read(p, &elemIP);

        char* elemPort;
        Packet_Read(p, &elemPort);

        // agrego a los peer conocidos temporales
        // ya que no puedo modificar GossipPeers mientras lo estoy iterando
        _addToIPPortDict(storages->NewDiscoveries, elemMemId, elemIP, elemPort);

        // si no esta en blacklist, agrego a la tabla de gossiping
        if (!hashmap_has_key(storages->Blacklist, elemMemId))
        {
            pthread_rwlock_wrlock(&GossipTableLock);

            _addToGossipList(elemMemId, elemIP, elemPort);

            pthread_rwlock_unlock(&GossipTableLock);
        }

        Free(elemPort);
        Free(elemIP);
    }

    Packet_Destroy(p);

    // me desconecto al terminar el gossip
    Socket_Destroy(s);
}

static void _addToKnownPeers(char const* key, void* value)
{
    GossipPeer* const gp = value;
    // me fijo primero si ya tengo el peer

    GossipPeer* const old = dictionary_get(GossipPeers, key);
    if (old)
    {
        // no hay cambios
        if (old->MemId == gp->MemId)
            return;

        // en caso de tenerlo, destruyo el viejo para poder actualizar el dato
        dictionary_remove_and_destroy(GossipPeers, key, Free);
    }
    else
        LISSANDRA_LOG_DEBUG("GOSSIP: Descubierta nueva memoria en %s:%s! (MemId: %u)", gp->IP, gp->Port, gp->MemId);

    _addToIPPortDict(GossipPeers, gp->MemId, gp->IP, gp->Port);
}

static void _addNewGossipsToIterateList(int _, void* value)
{
    (void) _;

    GossipPeer* peer = value;
    if (_IPPortDictHasKey(GossipPeers, peer->IP, peer->Port))
        return;

    _addToIPPortDict(GossipPeers, peer->MemId, peer->IP, peer->Port);
}

static void* _gossipWorkerTh(void* pt)
{
    LISSANDRA_LOG_DEBUG("GOSSIP: Iniciando gossiping...");

    // primero cualquier memoria que hayamos descubierto antes del gossiping (porque otra memoria nos gossipeo primero)
    // se agrega a la lista de pares conocidos
    pthread_rwlock_rdlock(&GossipTableLock);
    hashmap_iterate(GossipingTable, _addNewGossipsToIterateList);
    pthread_rwlock_unlock(&GossipTableLock);

    // temporal, almacena todas las memorias obtenidas durante gossip
    t_dictionary* newDiscoveries = dictionary_create();

    // blacklist para no aceptar memIds que descubri que estaban desconectadas durante esta iteracion
    t_hashmap* blacklist = hashmap_create();

    IterateParams p =
    {
        .NewDiscoveries = newDiscoveries,
        .Blacklist = blacklist
    };

    // hacer gossip con cada una de las memorias en mi lista de seeds, actualizando los nuevos discoveries en el temporal
    dictionary_iterator_with_data(GossipPeers, _iterateSuccessfulMemories, &p);

    // mergear los temporales con la lista de seeds
    dictionary_iterator(newDiscoveries, _addToKnownPeers);

    // uso como set
    hashmap_destroy(blacklist);

    dictionary_destroy_and_destroy_elements(newDiscoveries, Free);

    // aviso que termine para que el siguiente pueda ejecutar
    PeriodicTimer_SetEnabled(pt, true);
    return NULL;
}
