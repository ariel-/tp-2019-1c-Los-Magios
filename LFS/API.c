
#include "API.h"
#include "Compactador.h"
#include "Config.h"
#include "LissandraLibrary.h"
#include <Consistency.h>
#include <fcntl.h>
#include <Logger.h>
#include <Malloc.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

SelectResult api_select(char* nombreTabla, uint16_t key, char* value, uint64_t* timestamp)
{
    MSSleep(atomic_load(&confLFS.RETARDO));

    char path[PATH_MAX];
    generarPathTabla(nombreTabla, path);

    //Verificar que la tabla exista en el FS
    if (!existeDir(path))
    {
        LISSANDRA_LOG_ERROR("La tabla ingresada para SELECT: %s no existe en el File System", nombreTabla);
        return TableNotFound;
    }

    //Obtener la metadata asociada a dicha tabla
    t_describe infoTabla;
    if (!get_table_metadata(nombreTabla, &infoTabla))
        return TableNotFound;

    //Calcular cual es la particion que contiene dicho KEY
    uint16_t const particion = get_particion(infoTabla.partitions, key);
    char pathParticion[PATH_MAX];
    generarPathParticion(particion, path, pathParticion);

    // bloqueo sugerido compartido tabla (lectura)
    int fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        LISSANDRA_LOG_SYSERROR("open");
        return TableNotFound;
    }

    // dump bloquea select/compactacion durante el renombre y el bajado a archivo
    flock(fd, LOCK_SH);

    //Escanear la particion objetivo
    t_registro* resultadoParticion = Malloc(REGISTRO_SIZE);
    if (!scanParticion(pathParticion, key, resultadoParticion))
    {
        Free(resultadoParticion);
        resultadoParticion = NULL;
    }

    //Escanear todos los archivos temporales
    t_registro* resultadoTemporales = Malloc(REGISTRO_SIZE);
    if (!temporales_get_biggest_timestamp(path, key, resultadoTemporales))
    {
        Free(resultadoTemporales);
        resultadoTemporales = NULL;
    }

    //Escanear la memoria temporal de dicha tabla buscando la key deseada
    t_registro* resultadoMemtable = Malloc(REGISTRO_SIZE);
    if (!memtable_get_biggest_timestamp(nombreTabla, key, resultadoMemtable))
    {
        Free(resultadoMemtable);
        resultadoMemtable = NULL;
    }

    // quita el bloqueo sugerido
    close(fd);

    //Encontradas las entradas para dicha KEY, se retorna el valor con el Timestamp más grande
    t_registro const* maximo = get_newest(resultadoParticion, resultadoTemporales, resultadoMemtable);
    if (maximo)
    {
        strncpy(value, maximo->value, confLFS.TAMANIO_VALUE + 1);
        *timestamp = maximo->timestamp;
    }

    Free(resultadoMemtable);
    Free(resultadoTemporales);
    Free(resultadoParticion);

    if (!maximo)
        return KeyNotFound;
    return Ok;
}

uint8_t api_insert(char* nombreTabla, uint16_t key, char const* value, uint64_t timestamp)
{
    MSSleep(atomic_load(&confLFS.RETARDO));

    //Verifica si la tabla existe en el File System
    char path[PATH_MAX];
    generarPathTabla(nombreTabla, path);

    if (!existeDir(path))
    {
        LISSANDRA_LOG_ERROR("La tabla ingresada para INSERT: %s no existe en el File System", nombreTabla);
        return EXIT_FAILURE;
    }

    //Verifica si hay datos a dumpear, y si no existen aloca memoria
    memtable_new_elem(nombreTabla, key, value, timestamp);

    LISSANDRA_LOG_INFO("Se inserto un nuevo registro en la tabla %s", nombreTabla);
    return EXIT_SUCCESS;
}

uint8_t api_create(char* nombreTabla, uint8_t tipoConsistencia, uint16_t numeroParticiones, uint32_t compactionTime)
{
    MSSleep(atomic_load(&confLFS.RETARDO));

    char path[PATH_MAX];
    generarPathTabla(nombreTabla, path);

    //Evalua si existe la tabla
    //Si la tabla existe hace un log y muestra error
    //Si la tabla no existe la crea, crea su metadata y las particiones
    if (!existeDir(path))
    {
        mkdir(path, 0700);

        //Crea el path de la metadata de la tabla y le carga los datos
        char pathMetadataTabla[PATH_MAX];
        generarPathArchivo(nombreTabla, "Metadata", pathMetadataTabla);

        {
            FILE* metadata = fopen(pathMetadataTabla, "w");
            fprintf(metadata, "CONSISTENCY=%s\n", CriteriaString[tipoConsistencia].String);
            fprintf(metadata, "PARTITIONS=%d\n", numeroParticiones);
            fprintf(metadata, "COMPACTION_TIME=%d\n", compactionTime);
            fclose(metadata);
        }

        //Crea cada particion, le carga los datos y le asigna un bloque
        for (uint16_t j = 0; j < numeroParticiones; ++j)
        {
            char pathParticion[PATH_MAX];
            snprintf(pathParticion, PATH_MAX, "%s/%hu.bin", path, j);

            if (!existeArchivo(pathParticion))
            {
                size_t bloqueLibre;
                if (!buscarBloqueLibre(&bloqueLibre))
                {
                    LISSANDRA_LOG_ERROR("No hay espacio en el File System");

                    // api_drop(nombreTabla), porque si no puede crear todas las particiones
                    // para una tabla nueva, no tiene sentido que la tabla exista en si.
                    uint8_t resDrop = api_drop(nombreTabla);
                    if (resDrop == EXIT_FAILURE)
                        LISSANDRA_LOG_ERROR("Se produjo un error intentado borrar la tabla %s", nombreTabla);

                    return EXIT_FAILURE;
                }

                crearArchivoLFS(pathParticion, bloqueLibre);
            }
        }

        LISSANDRA_LOG_DEBUG("Se finalizo la creacion de la tabla");

        // creo un hilo compactador para la tabla
        agregarTablaCompactador(nombreTabla, compactionTime);
        return EXIT_SUCCESS;
    }
    else
    {
        LISSANDRA_LOG_ERROR("La tabla ya existe en el File System");
        return EXIT_FAILURE;
    }
}


//Verificar que la tabla exista en el file system.
//Eliminar directorio y todos los archivos de dicha tabla.

uint8_t api_drop(char* nombreTabla)
{
    MSSleep(atomic_load(&confLFS.RETARDO));

    char pathAbsoluto[PATH_MAX];
    generarPathTabla(nombreTabla, pathAbsoluto);

    LISSANDRA_LOG_INFO("Se esta borrando la tabla %s...", nombreTabla);

    if (!existeDir(pathAbsoluto))
    {
        LISSANDRA_LOG_ERROR("La tabla %s no existe...", nombreTabla);
        return EXIT_FAILURE;
    }

    //Se elimina la memtable de la tabla
    memtable_delete_table(nombreTabla);

    //Se elimina el hilo compactador de la tabla
    quitarTablaCompactador(nombreTabla);

    //Se eliminan los archivos de la tabla
    if (traverse_to_drop(pathAbsoluto) != 0 || rmdir(pathAbsoluto) != 0)
    {
        LISSANDRA_LOG_ERROR("Se produjo un error al intentar borrar la tabla: %s", nombreTabla);
        return EXIT_FAILURE;
    }

    LISSANDRA_LOG_INFO("Tabla %s borrada con exito", nombreTabla);
    return EXIT_SUCCESS;
}

void* api_describe(char* nombreTabla)
{
    MSSleep(atomic_load(&confLFS.RETARDO));

    if (!nombreTabla)
    {
        char dirTablas[PATH_MAX];
        snprintf(dirTablas, PATH_MAX, "%sTables", confLFS.PUNTO_MONTAJE);

        t_list* listTableMetadata = list_create();
        int resultado = traverse(dirTablas, listTableMetadata, NULL);
        if (resultado != EXIT_SUCCESS)
        {
            LISSANDRA_LOG_ERROR("DESCRIBE: Se produjo un error al recorrer el directorio /Tables");
            return NULL;
        }

        return listTableMetadata;
    }

    t_describe* desc = Malloc(sizeof(t_describe));
    if (!get_table_metadata(nombreTabla, desc))
    {
        Free(desc);
        return NULL;
    }

    return desc;
}
