
#include "Memtable.h"
#include "Config.h"
#include "LissandraLibrary.h"
#include <fcntl.h>
#include <libcommons/config.h>
#include <libcommons/dictionary.h>
#include <libcommons/string.h>
#include <Logger.h>
#include <Malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <unistd.h>

static t_dictionary* memtable = NULL;
static pthread_rwlock_t memtableMutex = PTHREAD_RWLOCK_INITIALIZER;

void _dump(void);

static void _delete_memtable_table(void* registros)
{
    Vector_Destruct(registros);
    Free(registros);
}

void memtable_create(void)
{
    memtable = dictionary_create();
    LISSANDRA_LOG_TRACE("Memtable creada");
}

void memtable_new_elem(char const* nombreTabla, uint16_t key, char const* value, uint64_t timestamp)
{
    t_registro* new = Malloc(REGISTRO_SIZE);
    new->key = key;
    new->timestamp = timestamp;
    strncpy(new->value, value, confLFS.TAMANIO_VALUE + 1);

    pthread_rwlock_wrlock(&memtableMutex);

    Vector* registros = dictionary_get(memtable, nombreTabla);
    if (!registros)
    {
        registros = Malloc(sizeof(Vector));
        Vector_Construct(registros, REGISTRO_SIZE, NULL, 0);

        dictionary_put(memtable, nombreTabla, registros);
    }

    Vector_push_back(registros, new);
    Free(new);

    pthread_rwlock_unlock(&memtableMutex);
}

bool memtable_get_biggest_timestamp(char const* nombreTabla, uint16_t key, t_registro* resultado)
{
    pthread_rwlock_rdlock(&memtableMutex);

    Vector* const registros = dictionary_get(memtable, nombreTabla);
    if (!registros)
    {
        pthread_rwlock_unlock(&memtableMutex);
        return NULL;
    }

    size_t const cantElementos = Vector_size(registros);

    t_registro* registroMayor = NULL;
    uint64_t timestampMayor = 0;

    for (size_t i = 0; i < cantElementos; ++i)
    {
        t_registro* const registro = Vector_at(registros, i);
        if (registro->key == key)
        {
            if (!registroMayor || registro->timestamp > timestampMayor)
            {
                timestampMayor = registro->timestamp;
                registroMayor = registro;
            }
        }
    }

    if (registroMayor)
        memcpy(resultado, registroMayor, REGISTRO_SIZE);

    pthread_rwlock_unlock(&memtableMutex);
    return registroMayor != NULL;
}

void memtable_delete_table(char const* nombreTabla)
{
    pthread_rwlock_wrlock(&memtableMutex);

    dictionary_remove_and_destroy(memtable, nombreTabla, _delete_memtable_table);

    pthread_rwlock_unlock(&memtableMutex);
}

static void _dump_table(char const* nombreTabla, void* registros)
{
    char pathTable[PATH_MAX];
    snprintf(pathTable, PATH_MAX, "%sTables/%s", confLFS.PUNTO_MONTAJE, nombreTabla);
    int fd = open(pathTable, O_RDONLY);
    if (fd == -1)
    {
        LISSANDRA_LOG_ERROR("La tabla hallada en la memtable no se encuentra en el File System... Esto no deberia estar pasando");
        return;
    }

    // bloqueo sugerido exclusivo (escritura, para no pisar el compactador o select)
    // "un dump tiene que bloquear tanto los select como las compactaciones"
    // Maximiliano Felice 15/07/2019 21:41
    flock(fd, LOCK_EX);

    size_t bloqueLibre;
    if (!buscarBloqueLibre(&bloqueLibre))
    {
        LISSANDRA_LOG_ERROR("No hay espacio en el File System. Se perderan datos...");
        close(fd);
        return;
    }

    Vector content;
    Vector_Construct(&content, sizeof(char), NULL, 0);

    for (size_t i = 0; i < Vector_size(registros); ++i)
    {
        t_registro* const registro = Vector_at(registros, i);

        size_t len = snprintf(NULL, 0, "%llu;%d;%s\n", registro->timestamp, registro->key, registro->value);
        char field[len + 1];
        snprintf(field, len + 1, "%llu;%d;%s\n", registro->timestamp, registro->key, registro->value);
        Vector_insert_range(&content, Vector_size(&content), field, field + len);
    }

    //Arma el path del archivo temporal. Si ese path ya existe, le busca nombre hasta encontrar uno libre
    char pathTemporal[PATH_MAX];
    for (size_t j = 0; snprintf(pathTemporal, PATH_MAX, "%sTables/%s/%d.tmp", confLFS.PUNTO_MONTAJE, nombreTabla, j), existeArchivo(pathTemporal); ++j);

    crearArchivoLFS(pathTemporal, bloqueLibre);
    escribirArchivoLFS(pathTemporal, Vector_data(&content), Vector_size(&content));

    Vector_Destruct(&content);

    // fin bloqueo
    close(fd);
}

void* memtable_dump_thread(void* pt)
{
    _dump();

    PeriodicTimer_SetEnabled(pt, true);
    return NULL;
}

void memtable_destroy(void)
{
    dictionary_destroy_and_destroy_elements(memtable, _delete_memtable_table);
}

/* PRIVATE */
void _dump(void)
{
    t_dictionary* oldMemtable;

    {
        //intercambio punteros
        pthread_rwlock_wrlock(&memtableMutex);
        oldMemtable = memtable;
        memtable = dictionary_create();
        pthread_rwlock_unlock(&memtableMutex);
    }

    //Itera tantas veces como tablas contó que había en ese momento en la memtable
    dictionary_iterator(oldMemtable, _dump_table);
    dictionary_destroy_and_destroy_elements(oldMemtable, _delete_memtable_table);
}
