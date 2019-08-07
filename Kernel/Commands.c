
#include "Commands.h"
#include "Metadata.h"
#include "PacketBuilders.h"
#include "Runner.h"
#include <ConsoleInput.h>
#include <File.h>
#include <Socket.h>
#include <Timer.h>

ScriptCommand const ScriptCommands[] =
{
    { "SELECT",   HandleSelect   },
    { "INSERT",   HandleInsert   },
    { "CREATE",   HandleCreate   },
    { "DESCRIBE", HandleDescribe },
    { "DROP",     HandleDrop     },
    { "JOURNAL",  HandleJournal  },
    { "ADD",      HandleAdd      },
    { "RUN",      HandleRun      },
    { "METRICS",  HandleMetrics  },
    { NULL,       NULL           }
};

// issue: 1433
// En el caso de que se caiga una Memoria mientras se esta realizando una request simplemente informá por consola
// del Kernel que esa operación no se pudo realizar correctamente por una falla y seguí con la siguiente en el script
// (o si es la última finalizá el script)
#define LOG_MEMORY_DOWN() LISSANDRA_LOG_ERROR("La memoria se desconecto mientras corria script")

bool HandleSelect(Vector const* args)
{
    //           cmd args
    //           0      1       2
    // sintaxis: SELECT <table> <key>

    if (Vector_size(args) != 3)
    {
        LISSANDRA_LOG_ERROR("SELECT: Uso - SELECT <tabla> <key>");
        return false;
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];
    char* const key = tokens[2];

    if (!ValidateTableName(table))
        return false;

    CriteriaType ct;
    if (!Metadata_Get(table, &ct))
    {
        LISSANDRA_LOG_ERROR("SELECT: Tabla %s no encontrada en metadata", table);
        return false;
    }

    uint16_t k;
    if (!ValidateKey(key, &k))
        return false;

    DBRequest dbr;
    dbr.TableName = table;
    dbr.Data.Select.Key = k;

    Memory* mem = Criteria_GetMemoryFor(ct, OP_SELECT, &dbr);
    if (!mem) // no hay memorias conectadas? criteria loguea el error
        return false;

    uint64_t requestTime = GetMSTime();
    Packet* p = Memory_SendRequestWithAnswer(mem, OP_SELECT, &dbr);

    // se desconecto la memoria! el sendrequest ya lo logueó
    if (!p)
    {
        LOG_MEMORY_DOWN();
        return true;
    }

    Criteria_AddMetric(ct, EVT_READ_LATENCY, GetMSTimeDiff(requestTime, GetMSTime()));

    switch (Packet_GetOpcode(p))
    {
        case MSG_SELECT:
            break;
        case MSG_ERR_MEM_FULL:
        {
            // usar valor igual (lo devuelve) pero además mandar un Journal
            // no uso el dispatcher, se envia a la memoria que me dió el error.
            Memory_SendRequest(mem, OP_JOURNAL, NULL /*unused*/);
            break;
        }
        case MSG_ERR_KEY_NOT_FOUND:
            LISSANDRA_LOG_ERROR("SELECT: key %hu no encontrada", k);
            Packet_Destroy(p);
            return true; // no se toma como fallo de script que una clave no exista (#1406)
        case MSG_ERR_TABLE_NOT_EXISTS:
            LISSANDRA_LOG_FATAL("SELECT: tabla %s no existe en el FS! Esto no deberia estar pasando...", table);
            Packet_Destroy(p);
            return false;
        default:
            LISSANDRA_LOG_FATAL("SELECT: recibido opcode no esperado %hu", Packet_GetOpcode(p));
            Packet_Destroy(p);
            return false;
    }

    char* value;
    Packet_Read(p, &value);

    LISSANDRA_LOG_INFO("SELECT: tabla: %s, key: %s, valor: %s", table, key, value);
    Free(value);
    Packet_Destroy(p);
    return true;
}

bool HandleInsert(Vector const* args)
{
    //           cmd args
    //           0      1       2     3
    // sintaxis: INSERT <table> <key> <value>

    // obs: INSERT desde kernel va sin Timestamp (ver #1355)
    if (Vector_size(args) != 4)
    {
        LISSANDRA_LOG_ERROR("INSERT: Uso - INSERT <table> <key> <value>");
        return false;
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];
    char* const key = tokens[2];
    char* const value = tokens[3];

    if (!ValidateTableName(table))
        return false;

    CriteriaType ct;
    if (!Metadata_Get(table, &ct))
    {
        LISSANDRA_LOG_ERROR("INSERT: Tabla %s no encontrada en metadata", table);
        return false;
    }

    uint16_t k;
    if (!ValidateKey(key, &k))
        return false;

    DBRequest dbr;
    dbr.TableName = table;
    dbr.Data.Insert.Key = k;
    dbr.Data.Insert.Value = value;

    Memory* mem = Criteria_GetMemoryFor(ct, OP_INSERT, &dbr);
    if (!mem) // no hay memorias conectadas? criteria loguea el error
        return false;

    uint64_t requestTime = GetMSTime();
    Packet* p = Memory_SendRequestWithAnswer(mem, OP_INSERT, &dbr);

    // se desconecto la memoria! el sendrequest ya lo logueó
    if (!p)
    {
        LOG_MEMORY_DOWN();
        return true;
    }

    switch (Packet_GetOpcode(p))
    {
        case MSG_INSERT_RESPUESTA:
            break;
        case MSG_ERR_VALUE_TOO_LONG:
            LISSANDRA_LOG_ERROR("INSERT: El valor '%s' es demasiado largo!", value);
            Packet_Destroy(p);
            return false;
        case MSG_ERR_MEM_FULL:
        {
            // se debe vaciar la memoria e intentar el request nuevamente. Esto aumenta la latencia de escritura.
            do
            {
                Packet_Destroy(p);

                Memory_SendRequest(mem, OP_JOURNAL, NULL /*unused*/);

                p = Memory_SendRequestWithAnswer(mem, OP_INSERT, &dbr);
            } while (Packet_GetOpcode(p) == MSG_ERR_MEM_FULL);
            break;
        }
        default:
            LISSANDRA_LOG_FATAL("INSERT: recibido opcode no esperado %hu", Packet_GetOpcode(p));
            Packet_Destroy(p);
            return false;
    }

    Criteria_AddMetric(ct, EVT_WRITE_LATENCY, GetMSTimeDiff(requestTime, GetMSTime()));

    LISSANDRA_LOG_INFO("INSERT: tabla: %s, key: %s, valor: %s", table, key, value);
    Packet_Destroy(p);
    return true;
}

bool HandleCreate(Vector const* args)
{
    //           cmd args
    //           0      1       2          3            4
    // sintaxis: CREATE <table> <criteria> <partitions> <compaction_time>

    if (Vector_size(args) != 5)
    {
        LISSANDRA_LOG_ERROR("CREATE: Uso - CREATE <tabla> <criterio> <particiones> <tiempo entre compactaciones>");
        return false;
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];
    char* const criteria = tokens[2];
    char* const partitions = tokens[3];
    char* const compaction_time = tokens[4];

    if (!ValidateTableName(table))
        return false;

    if (Metadata_Get(table, NULL))
    {
        LISSANDRA_LOG_ERROR("CREATE: Tabla %s ya existe en metadata!", table);
        return false;
    }

    CriteriaType ct;
    if (!CriteriaFromString(criteria, &ct))
        return false;

    DBRequest dbr;
    dbr.TableName = table;
    dbr.Data.Create.Consistency = ct;
    dbr.Data.Create.Partitions = strtoul(partitions, NULL, 10);
    dbr.Data.Create.CompactTime = strtoul(compaction_time, NULL, 10);

    Memory* mem = Criteria_GetMemoryFor(ct, OP_CREATE, &dbr);
    if (!mem) // no hay memorias conectadas? criteria loguea el error
        return false;

    Packet* p = Memory_SendRequestWithAnswer(mem, OP_CREATE, &dbr);
    // se desconecto la memoria! el sendrequest ya lo logueó
    if (!p)
    {
        LOG_MEMORY_DOWN();
        return true;
    }

    if (Packet_GetOpcode(p) != MSG_CREATE_RESPUESTA)
    {
        LISSANDRA_LOG_FATAL("CREATE: recibido opcode no esperado %hu", Packet_GetOpcode(p));
        Packet_Destroy(p);
        return false;
    }

    uint8_t res;
    Packet_Read(p, &res);
    Packet_Destroy(p);

    if (res == EXIT_FAILURE)
    {
        LISSANDRA_LOG_ERROR("CREATE: error al crear tabla %s!", table);
        return false;
    }

    Metadata_Add(table, ct);
    LISSANDRA_LOG_INFO("CREATE: tabla: %s, consistencia: %s, particiones: %s, tiempo compactacion: %s", table, criteria,
                       partitions, compaction_time);
    return true;
}

bool HandleDescribe(Vector const* args)
{
    //           cmd args
    //           0        1
    // sintaxis: DESCRIBE [name]

    if (Vector_size(args) > 2)
    {
        LISSANDRA_LOG_ERROR("DESCRIBE: Uso - DESCRIBE [tabla]");
        return false;
    }

    char** const tokens = Vector_data(args);

    char* table = NULL;
    if (Vector_size(args) == 2)
    {
        table = tokens[1];
        if (!ValidateTableName(table))
            return false;
    }

    DBRequest dbr;
    dbr.TableName = table;

    // para el describe se utiliza cualquier memoria (a menos que tenga metadatos, en cuyo caso se envia al criterio de tabla)
    CriteriaType ct = CRITERIA_ANY;
    if (table)
        (void) Metadata_Get(table, &ct);

    Memory* mem = Criteria_GetMemoryFor(ct, OP_DESCRIBE, &dbr);
    if (!mem) // no hay memorias conectadas? criteria loguea el error
        return false;

    Packet* p = Memory_SendRequestWithAnswer(mem, OP_DESCRIBE, &dbr);
    // se desconecto la memoria! el sendrequest ya lo logueó
    if (!p)
    {
        LOG_MEMORY_DOWN();
        return true;
    }

    if (Packet_GetOpcode(p) != MSG_ERR_TABLE_NOT_EXISTS && Packet_GetOpcode(p) != MSG_DESCRIBE && Packet_GetOpcode(p) != MSG_DESCRIBE_GLOBAL)
    {
        LISSANDRA_LOG_FATAL("DESCRIBE: recibido opcode no esperado %hu", Packet_GetOpcode(p));
        Packet_Destroy(p);
        return false;
    }

    Metadata_Update(table, p);
    Packet_Destroy(p);
    return true;
}

bool HandleDrop(Vector const* args)
{
    //           cmd args
    //           0    1
    // sintaxis: DROP <name>

    if (Vector_size(args) != 2)
    {
        LISSANDRA_LOG_ERROR("DROP: Uso - DROP <tabla>");
        return false;
    }

    char** const tokens = Vector_data(args);

    char* const table = tokens[1];
    if (!ValidateTableName(table))
        return false;

    CriteriaType ct;
    if (!Metadata_Get(table, &ct))
    {
        LISSANDRA_LOG_ERROR("DROP: Tabla %s no encontrada en metadata", table);
        return false;
    }

    DBRequest dbr;
    dbr.TableName = table;

    Memory* mem = Criteria_GetMemoryFor(ct, OP_DROP, &dbr);
    if (!mem) // no hay memorias conectadas? criteria loguea el error
        return false;

    Packet* p = Memory_SendRequestWithAnswer(mem, OP_DROP, &dbr);
    // se desconecto la memoria! el sendrequest ya lo logueó
    if (!p)
    {
        LOG_MEMORY_DOWN();
        return true;
    }

    if (Packet_GetOpcode(p) != MSG_DROP_RESPUESTA)
    {
        LISSANDRA_LOG_FATAL("DROP: recibido opcode no esperado %hu", Packet_GetOpcode(p));
        Packet_Destroy(p);
        return false;
    }

    uint8_t res;
    Packet_Read(p, &res);
    Packet_Destroy(p);

    if (res == EXIT_FAILURE)
    {
        LISSANDRA_LOG_ERROR("DROP: error al dropear tabla %s!", table);
        return false;
    }

    return true;
}

bool HandleJournal(Vector const* args)
{
    //           cmd args
    //           0
    // sintaxis: JOURNAL

    if (Vector_size(args) != 1)
    {
        LISSANDRA_LOG_ERROR("JOURNAL: Uso - JOURNAL");
        return false;
    }

    // broadcast a todos los criterios
    Criterias_BroadcastJournal();
    return true;
}

bool HandleAdd(Vector const* args)
{
    //           cmd args
    //           0   1      2   3  4
    // sintaxis: ADD MEMORY <n> TO <criteria>

    if (Vector_size(args) != 5)
    {
        LISSANDRA_LOG_ERROR("ADD: Uso - ADD MEMORY <n> TO <criterio>");
        return false;
    }

    char** const tokens = Vector_data(args);

    char* const magic = tokens[1];    // MEMORY
    char* const idx = tokens[2];      // n
    char* const magic2 = tokens[3];   // TO
    char* const criteria = tokens[4]; // criteria

    if (strcmp(magic, "MEMORY") != 0)
    {
        LISSANDRA_LOG_ERROR("ADD: Uso - ADD MEMORY <n> TO <criterio>");
        return false;
    }

    uint32_t memIdx = strtoul(idx, NULL, 10);
    if (!memIdx || !Criteria_MemoryExists(memIdx))
    {
        LISSANDRA_LOG_ERROR("ADD: Memoria %s no válida", idx);
        return false;
    }

    if (strcmp(magic2, "TO") != 0)
    {
        LISSANDRA_LOG_ERROR("ADD: Uso - ADD MEMORY <n> TO <criterio>");
        return false;
    }

    CriteriaType type;
    if (!CriteriaFromString(criteria, &type))
        return false;

    Criteria_AddMemory(type, memIdx);
    return true;
}

bool HandleRun(Vector const* args)
{
    //           cmd args
    //           0   1
    // sintaxis: RUN <path>
    if (Vector_size(args) != 2)
    {
        LISSANDRA_LOG_ERROR("RUN: Cantidad de parámetros incorrecta.");
        return false;
    }

    char** const tokens = Vector_data(args);

    char* const fileName = tokens[1];
    File* script = file_open(fileName, F_OPEN_READ);
    if (!file_is_open(script))
    {
        LISSANDRA_LOG_ERROR("RUN: Archivo no válido: %s", fileName);
        file_close(script);
        return false;
    }

    Runner_AddScript(fileName, script);
    return true;
}

bool HandleMetrics(Vector const* args)
{
    //           cmd
    //           0
    // sintaxis: METRICS
    if (Vector_size(args) != 1)
    {
        LISSANDRA_LOG_ERROR("RUN: Cantidad de parámetros incorrecta.");
        return false;
    }

    Criterias_Report(NULL);
    return true;
}
