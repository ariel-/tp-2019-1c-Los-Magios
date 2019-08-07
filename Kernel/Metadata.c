
#include "Metadata.h"
#include <libcommons/dictionary.h>
#include <Malloc.h>
#include <Opcodes.h>
#include <Packet.h>
#include <pthread.h>

typedef struct
{
    CriteriaType ct;
} MDEntry;

static t_dictionary* Metadata;
static pthread_rwlock_t MetadataLock = PTHREAD_RWLOCK_INITIALIZER;

static void _addMetadata(char const* name, CriteriaType ct);

void Metadata_Init(void)
{
    Metadata = dictionary_create();
}

void Metadata_Add(char const* name, CriteriaType ct)
{
    pthread_rwlock_wrlock(&MetadataLock);

    _addMetadata(name, ct);

    pthread_rwlock_unlock(&MetadataLock);
}

bool Metadata_Get(char const* name, CriteriaType* ct)
{
    MDEntry* entry;

    pthread_rwlock_rdlock(&MetadataLock);
    entry = dictionary_get(Metadata, name);
    pthread_rwlock_unlock(&MetadataLock);

    if (!entry)
        return false;

    if (ct)
        *ct = entry->ct;
    return true;
}

void Metadata_Update(char const* tableName, Packet* p)
{
    // bloquear mientras se actualiza porque podrian entrar nuevos request
    pthread_rwlock_wrlock(&MetadataLock);

    // limpiar metadata de tabla si es una sola (podria haber sido dropeada)
    if (tableName)
        dictionary_remove_and_destroy(Metadata, tableName, Free);

    uint32_t num = 1;
    switch (Packet_GetOpcode(p))
    {
        case MSG_ERR_TABLE_NOT_EXISTS:
            num = 0;
            break;
        case MSG_DESCRIBE_GLOBAL:
            // el global actualiza metadatos (limpiar tabla)
            Packet_Read(p, &num);
            dictionary_clean_and_destroy_elements(Metadata, Free);
            break;
        default:
            break;
    }

    LISSANDRA_LOG_INFO("DESCRIBE: numero de tablas %u", num);

    for (uint32_t i = 0; i < num; ++i)
    {
        char* name;
        Packet_Read(p, &name);

        uint8_t type;
        Packet_Read(p, &type);

        uint16_t partitions;
        Packet_Read(p, &partitions);

        uint32_t compaction_time;
        Packet_Read(p, &compaction_time);

        LISSANDRA_LOG_INFO(
                "DESCRIBE: Tabla nº %u: nombre: %s, tipo consistencia %u, particiones: %u, tiempo entre compactaciones %u",
                i + 1, name, type, partitions, compaction_time);

        _addMetadata(name, type);
        Free(name);
    }

    pthread_rwlock_unlock(&MetadataLock);
}

void Metadata_Destroy(void)
{
    dictionary_destroy_and_destroy_elements(Metadata, Free);
}

/* PRIVATE */
static void _addMetadata(char const* name, CriteriaType ct)
{
    MDEntry* entry = dictionary_get(Metadata, name);
    if (entry)
        entry->ct = ct;
    else
    {
        entry = Malloc(sizeof(MDEntry));
        entry->ct = ct;
        dictionary_put(Metadata, name, entry);
    }
}
