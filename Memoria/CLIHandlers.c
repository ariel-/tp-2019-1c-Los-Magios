
#include "CLIHandlers.h"
#include "API.h"
#include "MainMemory.h"
#include <ConsoleInput.h>
#include <Consistency.h>
#include <stddef.h>

void HandleSelect(Vector const* args)
{
    //           cmd args
    //           0      1       2
    // sintaxis: SELECT <table> <key>

    if (Vector_size(args) != 3)
    {
        LISSANDRA_LOG_ERROR("SELECT: Uso - SELECT <tabla> <key>");
        return;
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];
    char* const key = tokens[2];

    if (!ValidateTableName(table))
        return;

    uint16_t k;
    if (!ValidateKey(key, &k))
        return;

    uint32_t maxValueLength = Memory_GetMaxValueLength();

    char* value = Malloc(maxValueLength + 1);
    switch (API_Select(table, k, value))
    {
        case MemoryFull:
            LISSANDRA_LOG_ERROR("SELECT: Memoria llena. Hacer JOURNAL!");
            // no hay break, devuelvo el valor pero no lo puedo almacenar en memoria
        case Ok:
            LISSANDRA_LOG_INFO("SELECT: %s", value);
            break;
        case KeyNotFound:
            LISSANDRA_LOG_ERROR("SELECT: clave %hu no encontrada!", k);
            break;
        case TableNotFound:
            LISSANDRA_LOG_ERROR("SELECT: tabla %s no existe!", table);
            break;
    }

    Free(value);
}

void HandleInsert(Vector const* args)
{
    //           cmd args
    //           0      1       2     3
    // sintaxis: INSERT <table> <key> <value>

    // obs: INSERT desde memoria va sin Timestamp (ver #1355)
    if (Vector_size(args) != 4)
    {
        LISSANDRA_LOG_ERROR("INSERT: Uso - INSERT <table> <key> <value>");
        return;
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];
    char* const key = tokens[2];
    char* const value = tokens[3];

    if (!ValidateTableName(table))
        return;

    uint16_t k;
    if (!ValidateKey(key, &k))
        return;

    switch (API_Insert(table, k, value))
    {
        case InsertOk:
            break;
        case InsertOverflow:
            break;
        case InsertFull:
            LISSANDRA_LOG_ERROR("INSERT: Memoria llena, valor no insertado. Hacer JOURNAL!");
            break;
    }
}

void HandleCreate(Vector const* args)
{
    //           cmd args
    //           0      1       2          3            4
    // sintaxis: CREATE <table> <criteria> <partitions> <compaction_time>

    if (Vector_size(args) != 5)
    {
        LISSANDRA_LOG_ERROR("CREATE: Uso - CREATE <tabla> <criterio> <particiones> <tiempo entre compactaciones>");
        return;
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];
    char* const criteria = tokens[2];
    char* const partitions = tokens[3];
    char* const compaction_time = tokens[4];

    if (!ValidateTableName(table))
        return;

    uint16_t parts = strtoul(partitions, NULL, 10);
    uint32_t compactTime = strtoul(compaction_time, NULL, 10);
    CriteriaType ct;
    if (!CriteriaFromString(criteria, &ct))
        return;

    API_Create(table, ct, parts, compactTime);
}

static void DescribeTable(void* md)
{
    TableMD* const p = md;

    LISSANDRA_LOG_INFO("DESCRIBE: Tabla %s", p->tableName);
    LISSANDRA_LOG_INFO("  Criterio: %s", CriteriaString[p->ct].String);
    LISSANDRA_LOG_INFO("  Particiones: %u", p->parts);
    LISSANDRA_LOG_INFO("  Tiempo entre Compactaciones: %u", p->compTime);
}

void HandleDescribe(Vector const* args)
{
    //           cmd args
    //           0        1
    // sintaxis: DESCRIBE [name]

    if (Vector_size(args) > 2)
    {
        LISSANDRA_LOG_ERROR("DESCRIBE: Uso - DESCRIBE [tabla]");
        return;
    }

    char** const tokens = Vector_data(args);

    char* table = NULL;
    if (Vector_size(args) == 2)
    {
        table = tokens[1];
        if (!ValidateTableName(table))
            return;
    }

    Vector v;
    Vector_Construct(&v, sizeof(TableMD), FreeMD, 0);

    if (!API_Describe(table, &v) && table)
        LISSANDRA_LOG_ERROR("DESCRIBE: tabla %s no encontrada!", table);

    Vector_iterate(&v, DescribeTable);

    Vector_Destruct(&v);
}

void HandleDrop(Vector const* args)
{
    //           cmd args
    //           0    1
    // sintaxis: DROP <name>

    if (Vector_size(args) != 2)
    {
        LISSANDRA_LOG_ERROR("DROP: Uso - DROP <tabla>");
        return;
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];
    if (!ValidateTableName(table))
        return;

    API_Drop(table);
}

void HandleJournal(Vector const* args)
{
    //           cmd args
    //           0
    // sintaxis: JOURNAL

    if (Vector_size(args) != 1)
    {
        LISSANDRA_LOG_ERROR("JOURNAL: Uso - JOURNAL");
        return;
    }

    API_Journal(NULL);
}
