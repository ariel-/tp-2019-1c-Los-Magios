
#include "Compactador.h"
#include "Config.h"
#include "LissandraLibrary.h"
#include <ConsoleInput.h>
#include <dirent.h>
#include <libcommons/dictionary.h>
#include <libcommons/hashmap.h>
#include <libcommons/string.h>
#include <Logger.h>
#include <Malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <Timer.h>

static void _escribirDiccionario(char const*, t_hashmap**, uint16_t);
static void _convertirATmpc(char*);
static void _guardarRegistroDiccionario(int, void*, void*);

static bool _esTemporal(char const*, t_hashmap**, uint16_t);
static bool _leerDatosTmpc(char const*, t_hashmap**, uint16_t);
static bool _borrarTmpc(char const*, t_hashmap**, uint16_t);

static bool _iterarDirectorioTabla(char*, bool(*)(char const*, t_hashmap**, uint16_t), t_hashmap**, uint16_t);

static void* _hiloCompactador(void*);
static void _terminarHilo(void*);

typedef struct
{
    pthread_t ThreadId;
    uint32_t TiempoCompactacion;
    char NombreTabla[NAME_MAX + 1];
} HiloCompactador;

static t_dictionary* hilosCompactador = NULL;
static pthread_mutex_t timersMutex = PTHREAD_MUTEX_INITIALIZER;

void inicializarCompactador(void)
{
    hilosCompactador = dictionary_create();
}

void agregarTablaCompactador(char const* nombreTabla, uint32_t tiempoCompactaciones)
{
    pthread_mutex_lock(&timersMutex);

    HiloCompactador* new = Malloc(sizeof(HiloCompactador));
    new->TiempoCompactacion = tiempoCompactaciones;
    snprintf(new->NombreTabla, NAME_MAX + 1, "%s", nombreTabla);

    pthread_create(&new->ThreadId, NULL, _hiloCompactador, new);

    dictionary_put(hilosCompactador, nombreTabla, new);

    pthread_mutex_unlock(&timersMutex);
}

void quitarTablaCompactador(char const* nombreTabla)
{
    pthread_mutex_lock(&timersMutex);

    dictionary_remove_and_destroy(hilosCompactador, nombreTabla, _terminarHilo);

    pthread_mutex_unlock(&timersMutex);
}

void compactar(char* nombreTabla)
{
    //los datos del struct en LissandraLibrary.h
    t_describe infoTabla;
    if (!get_table_metadata(nombreTabla, &infoTabla))
        return;

    // no hay temporales? no compactamos nada!
    if (!_iterarDirectorioTabla(nombreTabla, _esTemporal, NULL, 0))
    {
        LISSANDRA_LOG_TRACE("COMPACTADOR: Tabla '%s': no hay temporales. Nada para hacer.", nombreTabla);
        return;
    }

    char pathTabla[PATH_MAX];
    generarPathTabla(nombreTabla, pathTabla);

    DIR* dir = opendir(pathTabla);
    if (!dir)
    {
        LISSANDRA_LOG_ERROR("COMPACTADOR: Directorio de tabla %s no encontrado... esto no deberia estar pasando!", nombreTabla);
        return;
    }

    uint64_t blockedTime = 0;
    uint64_t curTime;

    // mini SC (renombre a .tmpc)
    {
        curTime = GetMSTime();

        flock(dirfd(dir), LOCK_EX);
        _convertirATmpc(nombreTabla);
        flock(dirfd(dir), LOCK_UN);

        blockedTime += GetMSTimeDiff(curTime, GetMSTime());
    }

    uint16_t const numParticiones = infoTabla.partitions;
    t_hashmap* clavesCompactadas[numParticiones];
    for (uint16_t i = 0; i < numParticiones; ++i)
    {
        clavesCompactadas[i] = hashmap_create();

        char pathParticion[PATH_MAX];
        generarPathParticion(i, pathTabla, pathParticion);

        char* contenido = leerArchivoLFS(pathParticion);
        if (!contenido)
            continue;

        _escribirDiccionario(contenido, clavesCompactadas, numParticiones);
        Free(contenido);
    }

    _iterarDirectorioTabla(nombreTabla, _leerDatosTmpc, clavesCompactadas, numParticiones);

    // SC (bajada de nuevos .bin)
    {
        curTime = GetMSTime();

        flock(dirfd(dir), LOCK_EX);

        _iterarDirectorioTabla(nombreTabla, _borrarTmpc, NULL, 0);

        for (uint16_t i = 0; i < numParticiones; ++i)
        {
            Vector content;
            Vector_Construct(&content, sizeof(char), NULL, 0);

            hashmap_iterate_with_data(clavesCompactadas[i], _guardarRegistroDiccionario, &content);

            char pathParticion[PATH_MAX];
            generarPathParticion(i, pathTabla, pathParticion);

            escribirArchivoLFS(pathParticion, Vector_data(&content), Vector_size(&content));
            Vector_Destruct(&content);
        }

        flock(dirfd(dir), LOCK_UN);

        blockedTime += GetMSTimeDiff(curTime, GetMSTime());
    }

    for (uint16_t i = 0; i < numParticiones; ++i)
        hashmap_destroy_and_destroy_elements(clavesCompactadas[i], Free);

    closedir(dir);

    LISSANDRA_LOG_DEBUG("COMPACTADOR: Compactación de '%s' terminada. Tiempo bloqueo: %" PRIu64 "ms.", nombreTabla, blockedTime);
}

void terminarCompactador(void)
{
    dictionary_destroy_and_destroy_elements(hilosCompactador, _terminarHilo);
}

static void* _hiloCompactador(void* hiloCompactador)
{
    HiloCompactador* const hilo = hiloCompactador;
    while (true)
    {
        MSSleep(hilo->TiempoCompactacion);
        compactar(hilo->NombreTabla);
    }

    return NULL;
}

static void _terminarHilo(void* value)
{
    HiloCompactador* const hilo = value;
    pthread_cancel(hilo->ThreadId);

    void* retVal;
    pthread_join(hilo->ThreadId, &retVal);
    if (retVal != PTHREAD_CANCELED)
        LISSANDRA_LOG_ERROR("Se produjo un error al cancelar el hilo compactador de %s!", hilo->NombreTabla);

    Free(hilo);
}

static void _escribirDiccionario(char const* contenido, t_hashmap** diccionarios, uint16_t numParticiones)
{
    Vector registros = string_split(contenido, "\n");

    char** const tokens = Vector_data(&registros);
    for (size_t i = 0; i < Vector_size(&registros); ++i)
    {
        char* const registro = tokens[i];

        Vector campos = string_split(registro, ";");
        // timestamp;key;value
        char** const fields = Vector_data(&campos);
        uint64_t const timestamp = strtoull(fields[0], NULL, 10);

        uint16_t keyRegistro;
        if (!ValidateKey(fields[1], &keyRegistro))
        {
            LISSANDRA_LOG_ERROR("Error! clave invalida %s en archivo!!", fields[1]);
            Vector_Destruct(&campos);
            continue;
        }

        uint16_t const particion = get_particion(numParticiones, keyRegistro);
        t_hashmap* const diccionario = diccionarios[particion];

        char* const value = fields[2];
        t_registro* entradaDiccionario = hashmap_get(diccionario, keyRegistro);
        if (!entradaDiccionario)
        {
            entradaDiccionario = Malloc(REGISTRO_SIZE);

            entradaDiccionario->key = keyRegistro;
            entradaDiccionario->timestamp = timestamp;
            strncpy(entradaDiccionario->value, value, confLFS.TAMANIO_VALUE + 1);

            hashmap_put(diccionario, keyRegistro, entradaDiccionario);
        }
        else if (entradaDiccionario->timestamp < timestamp)
        {
            entradaDiccionario->timestamp = timestamp;
            strncpy(entradaDiccionario->value, value, confLFS.TAMANIO_VALUE + 1);
        }

        Vector_Destruct(&campos);
    }

    Vector_Destruct(&registros);
}

static bool _iterarDirectorioTabla(char* tabla, bool(*funcion)(char const*, t_hashmap**, uint16_t), t_hashmap** diccs, uint16_t numParticiones)
{
    char pathTabla[PATH_MAX];
    generarPathTabla(tabla, pathTabla);

    bool result = false;
    struct dirent* entry;

    DIR* dir = opendir(pathTabla);
    if (!dir)
        return false;

    while ((entry = readdir(dir)))
    {
        char pathCompleto[PATH_MAX];
        snprintf(pathCompleto, PATH_MAX, "%s/%s", pathTabla, entry->d_name);

        result = funcion(pathCompleto, diccs, numParticiones) || result;
    }
    closedir(dir);

    return result;
}

static bool _renombrarArchivoATmpc(char const* path, t_hashmap** _, uint16_t __)
{
    (void) _;
    (void) __;

    if (string_ends_with(path, ".tmp"))
    {
        char pathNuevoTemp[PATH_MAX];
        snprintf(pathNuevoTemp, PATH_MAX, "%sc", path);

        rename(path, pathNuevoTemp);
    }

    return true;
}

static void _convertirATmpc(char* tabla)
{
    _iterarDirectorioTabla(tabla, _renombrarArchivoATmpc, NULL, 0);
}

static bool _esTemporal(char const* path, t_hashmap** _, uint16_t __)
{
    (void) _;
    (void) __;

    return string_ends_with(path, ".tmp");
}

static bool _leerDatosTmpc(char const* path, t_hashmap** diccionarios, uint16_t numParticiones)
{
    if (!string_ends_with(path, ".tmpc"))
        return false;

    char* contenido = leerArchivoLFS(path);
    if (!contenido)
        return false;

    _escribirDiccionario(contenido, diccionarios, numParticiones);
    Free(contenido);

    return true;
}

static bool _borrarTmpc(char const* path, t_hashmap** _, uint16_t __)
{
    (void) _;
    (void) __;

    if (!string_ends_with(path, ".tmpc"))
        return false;

    borrarArchivoLFS(path);
    return true;
}

static void _guardarRegistroDiccionario(int key, void* value, void* content)
{
    t_registro* const registro = value;

    size_t len = snprintf(NULL, 0, "%llu;%d;%s\n", registro->timestamp, key, registro->value);
    char field[len + 1];
    snprintf(field, len + 1, "%llu;%d;%s\n", registro->timestamp, key, registro->value);
    Vector_insert_range(content, Vector_size(content), field, field + len);
}
