
#include "API.h"
#include "Config.h"
#include "FileSystemSocket.h"
#include "Frame.h"
#include "MainMemory.h"
#include <Logger.h>
#include <Opcodes.h>
#include <Packet.h>
#include <Socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Timer.h>

#define LOG_INVALID_OPCODE(fn) \
do { \
    LISSANDRA_LOG_FATAL(fn ": recibido opcode no esperado %hu", Packet_GetOpcode(p)); \
    Packet_Destroy(p); \
} while(false)

static void _ungratefulExit(void)
{
    LISSANDRA_LOG_FATAL("Se desconectó el FileSystem!! Saliendo...");
    exit(EXIT_FAILURE);
}

SelectResult API_Select(char const* tableName, uint16_t key, char* value)
{
    uint32_t const maxValueLength = Memory_GetMaxValueLength();

    // delay artificial acceso a memoria (read latency)
    MSSleep(ConfigMemoria.RETARDO_MEM);
    Frame* f = Memory_GetFrame(tableName, key);
    if (f)
    {
        *value = '\0';
        strncat(value, f->Value, maxValueLength);
        return Ok;
    }

    // delay artificial acceso FS
    MSSleep(ConfigMemoria.RETARDO_FS);

    // si llegué aca es porque no encuentro el valor asi que voy a ir a buscarlo al FS
    Packet* p = Packet_Create(LQL_SELECT, 16 + 2); // adivinar tamaño
    Packet_Append(p, tableName);
    Packet_Append(p, key);
    Socket_SendPacket(FileSystemSocket, p);
    Packet_Destroy(p);

    p = Socket_RecvPacket(FileSystemSocket);
    if (!p)
        _ungratefulExit();

    switch (Packet_GetOpcode(p))
    {
        case MSG_SELECT:
            break;
        case MSG_ERR_KEY_NOT_FOUND:
            Packet_Destroy(p);
            return KeyNotFound;
        case MSG_ERR_TABLE_NOT_EXISTS:
            Packet_Destroy(p);
            return TableNotFound;
        default:
            // deberia ser otra cosa pero para no confundir al llamante con un estado de "otro error"
            LOG_INVALID_OPCODE("SELECT");
            return Ok;
    }

    uint64_t timestamp;
    Packet_Read(p, &timestamp);

    char* fs_value;
    Packet_Read(p, &fs_value);
    snprintf(value, maxValueLength + 1, "%s", fs_value);
    Free(fs_value);
    Packet_Destroy(p);

    if (!Memory_InsertNewValue(tableName, timestamp, key, value))
        return MemoryFull;

    // delay artificial acceso a memoria (write latency)
    MSSleep(ConfigMemoria.RETARDO_MEM);
    return Ok;
}

InsertResult API_Insert(char const* tableName, uint16_t key, char const* value)
{
    size_t const maxLen = Memory_GetMaxValueLength();

    size_t len = strlen(value);
    if (len > Memory_GetMaxValueLength())
    {
        LISSANDRA_LOG_ERROR("El valor '%s' (longitud: %u) supera el máximo de %u caracteres!!", value, len, maxLen);
        return InsertOverflow;
    }

    LISSANDRA_LOG_DEBUG("INSERT %s %u %s", tableName, key, value);
    if (!Memory_UpsertValue(tableName, key, value))
        return InsertFull;

    // delay artificial acceso a memoria (write latency)
    MSSleep(ConfigMemoria.RETARDO_MEM);
    return InsertOk;
}

uint8_t API_Create(char const* tableName, CriteriaType consistency, uint16_t partitions, uint32_t compactionTime)
{
    Packet* p = Packet_Create(LQL_CREATE, 16 + 3 + 4 + 4); // adivinar tamaño
    Packet_Append(p, tableName);
    Packet_Append(p, (uint8_t) consistency);
    Packet_Append(p, partitions);
    Packet_Append(p, compactionTime);

    Socket_SendPacket(FileSystemSocket, p);
    Packet_Destroy(p);

    p = Socket_RecvPacket(FileSystemSocket);
    if (!p)
        _ungratefulExit();

    if (Packet_GetOpcode(p) != MSG_CREATE_RESPUESTA)
    {
        LOG_INVALID_OPCODE("CREATE");
        return false;
    }

    uint8_t createRes;
    Packet_Read(p, &createRes);
    Packet_Destroy(p);

    return createRes;
}

bool API_Describe(char const* tableName, Vector* results)
{
    bool result = true;
    Packet* p = Packet_Create(LQL_DESCRIBE, 16);
    Packet_Append(p, (bool) tableName);
    if (tableName)
        Packet_Append(p, tableName);

    Socket_SendPacket(FileSystemSocket, p);
    Packet_Destroy(p);

    uint32_t numTables;
    p = Socket_RecvPacket(FileSystemSocket);
    if (!p)
        _ungratefulExit();

    switch (Packet_GetOpcode(p))
    {
        case MSG_ERR_TABLE_NOT_EXISTS:
            if (tableName)
                LISSANDRA_LOG_ERROR("DESCRIBE: tabla %s no existe en FileSystem!", tableName);
            numTables = 0;
            result = false;
            break;
        case MSG_DESCRIBE:
            numTables = 1;
            break;
        case MSG_DESCRIBE_GLOBAL:
            Packet_Read(p, &numTables);
            break;
        default:
            LOG_INVALID_OPCODE("DESCRIBE");
            return true; /* si devuelvo false es que no encontre tabla */
    }

    Vector_reserve(results, numTables);
    for (uint32_t i = 0; i < numTables; ++i)
    {
        TableMD Metadata;
        Packet_Read(p, &Metadata.tableName);
        Packet_Read(p, &Metadata.ct);
        Packet_Read(p, &Metadata.parts);
        Packet_Read(p, &Metadata.compTime);

        Vector_push_back(results, &Metadata);
    }

    Packet_Destroy(p);
    return result;
}

uint8_t API_Drop(char const* tableName)
{
    Memory_EvictPages(tableName);

    Packet* p = Packet_Create(LQL_DROP, 16);
    Packet_Append(p, tableName);
    Socket_SendPacket(FileSystemSocket, p);
    Packet_Destroy(p);

    p = Socket_RecvPacket(FileSystemSocket);
    if (!p)
        _ungratefulExit();

    if (Packet_GetOpcode(p) != MSG_DROP_RESPUESTA)
    {
        LOG_INVALID_OPCODE("DROP");
        return false;
    }

    uint8_t dropRes;
    Packet_Read(p, &dropRes);
    Packet_Destroy(p);

    return dropRes;
}

static void Journal_Register(void* dirtyFrame)
{
    uint32_t const maxValueLength = Memory_GetMaxValueLength();

    DirtyFrame* const df = dirtyFrame;

    Packet* p = Packet_Create(LQL_INSERT, 50);
    Packet_Append(p, df->TableName);
    Packet_Append(p, df->Frame->Key);

    char value[maxValueLength + 1];
    *value = '\0';
    strncat(value, df->Frame->Value, maxValueLength);
    Packet_Append(p, value);

    Packet_Append(p, df->Frame->Timestamp);

    Socket_SendPacket(FileSystemSocket, p);
    Packet_Destroy(p);

    p = Socket_RecvPacket(FileSystemSocket);
    if (!p)
        _ungratefulExit();

    if (Packet_GetOpcode(p) != MSG_INSERT_RESPUESTA)
    {
        LOG_INVALID_OPCODE("JOURNAL_INSERT");
        return;
    }

    // En el caso que al momento de realizar el Journaling una tabla no exista,
    // deberá informar por archivo de log esta situación,
    // pero el proceso deberá actualizar correctamente las tablas que sí existen.
    uint8_t respuestaInsert;
    Packet_Read(p, &respuestaInsert);

    if (respuestaInsert == EXIT_FAILURE)
        LISSANDRA_LOG_WARN("JOURNAL: Intento de insertar key %u, valor '%s' en tabla %s no existente!", df->Frame->Key, value, df->TableName);
    else
        LISSANDRA_LOG_TRACE("JOURNAL: insertado key %u, valor '%s' en tabla %s", df->Frame->Key, value, df->TableName);

    Packet_Destroy(p);
}

void API_Journal(PeriodicTimer* pt)
{
    (void) pt;

    LISSANDRA_LOG_DEBUG("JOURNAL");
    Memory_DoJournal(Journal_Register);
}
