
#include "FileSystem.h"
#include "Compactador.h"
#include "Config.h"
#include "LissandraLibrary.h"
#include <dirent.h>
#include <fcntl.h>
#include <libcommons/config.h>
#include <Logger.h>
#include <Malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

char pathMetadataBitarray[PATH_MAX] = { 0 };

t_bitarray* bitArray = NULL;

static uint8_t* bitmap = NULL;
static size_t sizeBitArray = 0;

void iniciarFileSystem(void)
{
    char pathMetadata[PATH_MAX];
    snprintf(pathMetadata, PATH_MAX, "%sMetadata", confLFS.PUNTO_MONTAJE);
    LISSANDRA_LOG_INFO("Path Metadata %s...", pathMetadata);

    char pathBloques[PATH_MAX];
    snprintf(pathBloques, PATH_MAX, "%sBloques", confLFS.PUNTO_MONTAJE);
    LISSANDRA_LOG_INFO("Path Bloques %s...", pathBloques);

    char pathTablas[PATH_MAX];
    snprintf(pathTablas, PATH_MAX, "%sTables", confLFS.PUNTO_MONTAJE);
    LISSANDRA_LOG_INFO("Path Tables %s...", pathTablas);

    mkdirRecursivo(confLFS.PUNTO_MONTAJE);

    mkdir(pathMetadata, 0700);
    mkdir(pathBloques, 0700);
    mkdir(pathTablas, 0700);

    char metadataFile[PATH_MAX];
    snprintf(metadataFile, PATH_MAX, "%s/Metadata.bin", pathMetadata);

    if (existeArchivo(metadataFile))
    {
        t_config* configAux = config_create(metadataFile);
        size_t size = config_get_long_value(configAux, "BLOCK_SIZE");
        size_t bloques = config_get_long_value(configAux, "BLOCKS");
        LISSANDRA_LOG_INFO("Ya Existe un FS en ese punto de montaje con %d bloques de %d bytes de tamanio", bloques, size);
        if (size != confLFS.TAMANIO_BLOQUES || bloques != confLFS.CANTIDAD_BLOQUES)
        {
            confLFS.TAMANIO_BLOQUES = size;
            confLFS.CANTIDAD_BLOQUES = bloques;
        }
        config_destroy(configAux);
    }
    else
    {
        FILE* metadata = fopen(metadataFile, "w");
        fprintf(metadata, "BLOCK_SIZE=%d\n", confLFS.TAMANIO_BLOQUES);
        fprintf(metadata, "BLOCKS=%d\n", confLFS.CANTIDAD_BLOQUES);
        fprintf(metadata, "MAGIC_NUMBER=LISSANDRA\n");
        fclose(metadata);
    }

    sizeBitArray = confLFS.CANTIDAD_BLOQUES / 8;
    if (confLFS.CANTIDAD_BLOQUES % 8)
        ++sizeBitArray;

    snprintf(pathMetadataBitarray, PATH_MAX, "%s/Bitmap.bin", pathMetadata);

    // abrir
    int fd = open(pathMetadataBitarray, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        LISSANDRA_LOG_SYSERROR("open");
        exit(EXIT_FAILURE);
    }

    struct stat stats;
    if (fstat(fd, &stats) == -1)
    {
        LISSANDRA_LOG_SYSERROR("fstat");
        exit(EXIT_FAILURE);
    }

    size_t const fileSize = stats.st_size;
    if (fileSize != sizeBitArray)
    {
        LISSANDRA_LOG_ERROR("Bitmap.bin no tiene el tamaño correcto (%u bytes, esperaba %u). Corrigiendo...", fileSize, sizeBitArray);
        if (ftruncate(fd, sizeBitArray) == -1)
        {
            LISSANDRA_LOG_SYSERROR("ftruncate");
            exit(EXIT_FAILURE);
        }
    }

    bitmap = mmap(NULL, sizeBitArray, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (bitmap == MAP_FAILED)
    {
        LISSANDRA_LOG_SYSERROR("mmap");
        exit(EXIT_FAILURE);
    }

    close(fd);

    bitArray = bitarray_create_with_mode(bitmap, sizeBitArray, LSB_FIRST);
    if (dirIsEmpty(pathBloques))
    {
        for (size_t j = 0; j < confLFS.CANTIDAD_BLOQUES; ++j)
        {
            char pathBloque[PATH_MAX];
            snprintf(pathBloque, PATH_MAX, "%s/%d.bin", pathBloques, j);

            if (!existeArchivo(pathBloque))
            {
                FILE* bloque = fopen(pathBloque, "w");
                if (ftruncate(fileno(bloque), confLFS.TAMANIO_BLOQUES) < 0)
                    LISSANDRA_LOG_SYSERROR("ftruncate");
                fclose(bloque);
            }
        }
    }

    // crear hilos para las tablas que existen
    inicializarCompactador();

    DIR* dir = opendir(pathTablas);
    if (!dir)
    {
        LISSANDRA_LOG_FATAL("No pude abrir el directorio de tablas!");
        exit(EXIT_FAILURE);
    }

    struct dirent* entry;
    while ((entry = readdir(dir)))
    {
        if (*entry->d_name == '.')
            continue;

        t_describe tableMetadata;
        if (!get_table_metadata(entry->d_name, &tableMetadata))
        {
            LISSANDRA_LOG_FATAL("No se pudo encontrar metadatos para la tabla %s!", entry->d_name);
            exit(EXIT_FAILURE);
        }

        agregarTablaCompactador(tableMetadata.table, tableMetadata.compaction_time);
    }

    closedir(dir);

    LISSANDRA_LOG_TRACE("Se finalizo la creacion del File System");
}

void terminarFileSystem(void)
{
    terminarCompactador();
    bitarray_destroy(bitArray);
    munmap(bitmap, sizeBitArray);
}
