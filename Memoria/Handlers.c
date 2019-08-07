
#include "Handlers.h"
#include "API.h"
#include "Config.h"
#include "Gossip.h"
#include "MainMemory.h"
#include <Logger.h>
#include <Opcodes.h>
#include <Packet.h>
#include <Socket.h>

OpcodeHandlerFnType* const OpcodeTable[NUM_HANDLED_OPCODES] =
{
    // se conecta el kernel/otra memoria (gossip)
    HandleHandshakeOpcode,  // MSG_HANDSHAKE

    // 5 comandos basicos
    HandleSelectOpcode,     // LQL_SELECT
    HandleInsertOpcode,     // LQL_INSERT
    HandleCreateOpcode,     // LQL_CREATE
    HandleDescribeOpcode,   // LQL_DESCRIBE
    HandleDropOpcode,       // LQL_DROP

    // el kernel envia este query
    HandleJournalOpcode     // LQL_JOURNAL
};

void HandleHandshakeOpcode(Socket* s, Packet* p)
{
    uint8_t id;
    Packet_Read(p, &id);

    uint32_t const memId = ConfigMemoria.MEMORY_NUMBER;
    char const* const myPort = ConfigMemoria.PUERTO;

    if (id != KERNEL && id != MEMORIA)
    {
        LISSANDRA_LOG_ERROR("Se conecto un desconocido! (id %d)", id);
        return;
    }

    // protocolo compartido kernel y memoria
    {
        char* myIP;
        Packet_Read(p, &myIP);

        // me agrego a mi mismo
        Gossip_AddMemory(memId, myIP, myPort);

        Free(myIP);
    }

    // protocolo memoria only
    if (id == MEMORIA)
    {
        {
            uint32_t otherId;
            Packet_Read(p, &otherId);

            char* otherPort;
            Packet_Read(p, &otherPort);

            // agrego a la otra memoria
            Gossip_AddMemory(otherId, s->Address.HostIP, otherPort);

            Free(otherPort);
        }

        // agrego otras memorias en la tabla de gossiping
        uint32_t numItems;
        Packet_Read(p, &numItems);
        for (uint32_t i = 0; i < numItems; ++i)
        {
            uint32_t peerId;
            Packet_Read(p, &peerId);

            char* peerIP;
            Packet_Read(p, &peerIP);

            char* peerPort;
            Packet_Read(p, &peerPort);

            Gossip_AddMemory(peerId, peerIP, peerPort);

            Free(peerPort);
            Free(peerIP);
        }
    }

    // como respuesta, envio mi tabla actualizada
    Gossip_SendTable(s);
}

void HandleSelectOpcode(Socket* s, Packet* p)
{
    char* tableName;
    uint16_t key;

    Packet_Read(p, &tableName);
    Packet_Read(p, &key);

    uint32_t maxValueLength = Memory_GetMaxValueLength();

    char* value = Malloc(maxValueLength + 1);

    Packet* resp;
    switch (API_Select(tableName, key, value))
    {
        case Ok:
            resp = Packet_Create(MSG_SELECT, maxValueLength + 1);
            Packet_Append(resp, value);
            break;
        case KeyNotFound:
            resp = Packet_Create(MSG_ERR_KEY_NOT_FOUND, 0);
            break;
        case TableNotFound:
            resp = Packet_Create(MSG_ERR_TABLE_NOT_EXISTS, 0);
            break;
        case MemoryFull:
            resp = Packet_Create(MSG_ERR_MEM_FULL, maxValueLength + 1);
            Packet_Append(resp, value);
            break;
        default:
            LISSANDRA_LOG_ERROR("API_Select retornó valor inválido!!");
            return;
    }

    Socket_SendPacket(s, resp);
    Packet_Destroy(resp);

    Free(value);
    Free(tableName);
}

void HandleInsertOpcode(Socket* s, Packet* p)
{
    (void) s;

    char* tableName;
    uint16_t key;
    char* value;

    Packet_Read(p, &tableName);
    Packet_Read(p, &key);
    Packet_Read(p, &value);

    Opcodes opcode;
    switch (API_Insert(tableName, key, value))
    {
        case InsertOverflow:
            opcode = MSG_ERR_VALUE_TOO_LONG;
            break;
        case InsertFull:
            opcode = MSG_ERR_MEM_FULL;
            break;
        default:
            opcode = MSG_INSERT_RESPUESTA;
            break;
    }

    Free(tableName);
    Free(value);

    Packet* res = Packet_Create(opcode, 0);
    Socket_SendPacket(s, res);
    Packet_Destroy(res);
}

void HandleCreateOpcode(Socket* s, Packet* p)
{
    (void) s;

    char* tableName;
    uint8_t ct;
    uint16_t parts;
    uint32_t compactionTime;

    Packet_Read(p, &tableName);
    Packet_Read(p, &ct);
    Packet_Read(p, &parts);
    Packet_Read(p, &compactionTime);

    uint8_t createResult = API_Create(tableName, (CriteriaType) ct, parts, compactionTime);
    Free(tableName);

    Packet* res = Packet_Create(MSG_CREATE_RESPUESTA, 1);
    Packet_Append(res, createResult);
    Socket_SendPacket(s, res);
    Packet_Destroy(res);
}

static void AddToPacket(void* md, void* packet)
{
    TableMD* const p = md;
    Packet_Append(packet, p->tableName);
    Packet_Append(packet, p->ct);
    Packet_Append(packet, p->parts);
    Packet_Append(packet, p->compTime);
}

void HandleDescribeOpcode(Socket* s, Packet* p)
{
    bool tablePresent;
    Packet_Read(p, &tablePresent);

    char* tableName = NULL;
    if (tablePresent)
        Packet_Read(p, &tableName);

    Vector v;
    Vector_Construct(&v, sizeof(TableMD), FreeMD, 0);

    if (!API_Describe(tableName, &v))
    {
        Free(tableName);
        Vector_Destruct(&v);

        Packet* errPacket = Packet_Create(MSG_ERR_TABLE_NOT_EXISTS, 0);
        Socket_SendPacket(s, errPacket);
        Packet_Destroy(errPacket);
        return;
    }

    Opcodes opcode = MSG_DESCRIBE_GLOBAL;
    if (tableName)
        opcode = MSG_DESCRIBE;

    Packet* resp = Packet_Create(opcode, 100);
    if (opcode == MSG_DESCRIBE_GLOBAL)
        Packet_Append(resp, (uint32_t) Vector_size(&v));

    Vector_iterate_with_data(&v, AddToPacket, resp);
    Vector_Destruct(&v);

    Socket_SendPacket(s, resp);
    Packet_Destroy(resp);

    Free(tableName);
}

void HandleDropOpcode(Socket* s, Packet* p)
{
    (void) s;

    char* tableName;
    Packet_Read(p, &tableName);

    uint8_t dropResult = API_Drop(tableName);
    Free(tableName);

    Packet* res = Packet_Create(MSG_DROP_RESPUESTA, 1);
    Packet_Append(res, dropResult);
    Socket_SendPacket(s, res);
    Packet_Destroy(res);
}

void HandleJournalOpcode(Socket* s, Packet* p)
{
    (void) s;
    (void) p;

    API_Journal(NULL);
}
