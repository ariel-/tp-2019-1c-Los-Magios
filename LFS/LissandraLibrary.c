
#include "LissandraLibrary.h"
#include "Config.h"
#include "FileSystem.h"
#include "Memtable.h"
#include <Consistency.h>
#include <Console.h>
#include <ConsoleInput.h>
#include <dirent.h>
#include <EventDispatcher.h>
#include <fcntl.h>
#include <File.h>
#include <libcommons/config.h>
#include <libcommons/string.h>
#include <Opcodes.h>
#include <Packet.h>
#include <Socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <Threads.h>
#include <unistd.h>
#include <Timer.h>

//Variables
//static hace que las variables no se puedan referenciar desde otro .c utilizando 'extern'
//si lo desean cambiar, quitenlo
static Socket* sock_LFS = NULL;

static pthread_mutex_t bitarrayLock = PTHREAD_MUTEX_INITIALIZER;

void* atender_memoria(void* socketMemoria)
{
    while (ProcessRunning)
    {
        if (!Socket_HandlePacket(socketMemoria))
        {
            /// ya hace logs si hubo errores, cerrar conexión
            Socket_Destroy(socketMemoria);
            break;
        }
    }

    return NULL;
}

void memoria_conectar(Socket* fs, Socket* memoriaNueva)
{
    (void) fs;

    //Recibe el handshake
    Packet* p = Socket_RecvPacket(memoriaNueva);
    if (!p)
    {
        LISSANDRA_LOG_ERROR("Memoria se desconecto durante handshake!");
        return;
    }

    if (Packet_GetOpcode(p) != MSG_HANDSHAKE)
    {
        LISSANDRA_LOG_ERROR("HANDSHAKE: recibido opcode no esperado %hu", Packet_GetOpcode(p));
        Packet_Destroy(p);
        return;
    }

    uint8_t id;
    Packet_Read(p, &id);

    //----Recibo un handshake del cliente para ver si es una memoria
    if (id != MEMORIA)
    {
        LISSANDRA_LOG_ERROR("Se conecto un desconocido! (id %d)", id);
        Packet_Destroy(p);
        Socket_Destroy(memoriaNueva);
        return;
    }

    LISSANDRA_LOG_INFO("Se conecto una memoria en el socket: %d\n", memoriaNueva->_impl.Handle);

    Packet_Destroy(p);

    //Le envia a la memoria el TAMANIO_VALUE y el punto de montaje
    uint32_t const tamanioValue = confLFS.TAMANIO_VALUE;
    char const* const puntoMontaje = confLFS.PUNTO_MONTAJE;

    p = Packet_Create(MSG_HANDSHAKE_RESPUESTA, 50);
    Packet_Append(p, tamanioValue);
    Packet_Append(p, puntoMontaje);
    Socket_SendPacket(memoriaNueva, p);
    Packet_Destroy(p);

    //----Creo un hilo para cada memoria que se me conecta
    Threads_CreateDetached(atender_memoria, memoriaNueva);
}

void iniciar_servidor(void)
{
    //----Creo socket de LFS, hago el bind y comienzo a escuchar
    SocketOpts opts =
    {
        .SocketMode = SOCKET_SERVER,
        .ServiceOrPort = confLFS.PUERTO_ESCUCHA,
        .HostName = NULL,

        // cuando una memoria conecte, llamar a memoria_conectar
        .SocketOnAcceptClient = memoria_conectar,
    };
    sock_LFS = Socket_Create(&opts);

    if (!sock_LFS)
    {
        LISSANDRA_LOG_FATAL("No pudo iniciarse el socket de escucha!!");
        exit(1);
    }

    LISSANDRA_LOG_TRACE("Servidor LFS iniciado");

    EventDispatcher_AddFDI(sock_LFS);
}

void mkdirRecursivo(char const* path)
{
    char tmp[256];
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (char* p = tmp + 1; *p; ++p)
    {
        if (*p == '/')
        {
            *p = '\0';
            mkdir(tmp, 0700);
            *p = '/';
        }
    }

    mkdir(tmp, 0700);
}

bool existeArchivo(char const* path)
{
    File* file = file_open(path, F_OPEN_READ);
    bool res = file_is_open(file);

    file_close(file);
    return res;
}

bool existeDir(char const* pathDir)
{
    struct stat st = { 0 };

    //Si existe devuelve 0
    return stat(pathDir, &st) != -1;
}

void generarPathTabla(char* nombreTabla, char* buf)
{
    //Como los nombres de las tablas deben estar en uppercase, primero me aseguro de que así sea y luego genero el path de esa tabla
    string_to_upper(nombreTabla);
    snprintf(buf, PATH_MAX, "%sTables/%s", confLFS.PUNTO_MONTAJE, nombreTabla);
}

bool buscarBloqueLibre(size_t* bloqueLibre)
{
    pthread_mutex_lock(&bitarrayLock);

    size_t i = 0;
    for (; bitarray_test_bit(bitArray, i) && i < confLFS.CANTIDAD_BLOQUES; ++i);

    if (i < confLFS.CANTIDAD_BLOQUES)
    {
        *bloqueLibre = i;

        // marca el bloque como ocupado
        bitarray_set_bit(bitArray, i);
    }

    pthread_mutex_unlock(&bitarrayLock);
    return i < confLFS.CANTIDAD_BLOQUES;
}

void generarPathBloque(size_t numBloque, char* buf)
{
    snprintf(buf, PATH_MAX, "%sBloques/%d.bin", confLFS.PUNTO_MONTAJE, numBloque);
}

void escribirValorBitarray(bool valor, size_t pos)
{
    pthread_mutex_lock(&bitarrayLock);

    if (valor)
        bitarray_set_bit(bitArray, pos);
    else
        bitarray_clean_bit(bitArray, pos);

    pthread_mutex_unlock(&bitarrayLock);

    LISSANDRA_LOG_TRACE("FS: Marcado bloque %u como %s", pos, valor ? "ocupado" : "libre");
}

bool get_table_metadata(char const* tabla, t_describe* res)
{
    char pathMetadata[PATH_MAX];
    generarPathArchivo(tabla, "Metadata", pathMetadata);

    t_config* contenido = config_create(pathMetadata);
    if (!contenido)
        return false;

    char const* consistency = config_get_string_value(contenido, "CONSISTENCY");
    CriteriaType ct;
    if (!CriteriaFromString(consistency, &ct)) // error!
        return false;

    uint16_t partitions = config_get_int_value(contenido, "PARTITIONS");
    uint32_t compaction_time = config_get_long_value(contenido, "COMPACTION_TIME");
    config_destroy(contenido);

    snprintf(res->table, NAME_MAX + 1, "%s", tabla);
    res->consistency = (uint8_t) ct;
    res->partitions = partitions;
    res->compaction_time = compaction_time;

    return true;
}

int traverse(char const* fn, t_list* lista, char const* tabla)
{
    DIR* dir = opendir(fn);
    if (!dir)
    {
        LISSANDRA_LOG_ERROR("ERROR: La ruta %s especificada es invalida!", fn);
        return EXIT_FAILURE;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)))
    {
        if (*entry->d_name != '.')
        {
            struct stat info;

            char path[PATH_MAX];
            snprintf(path, PATH_MAX, "%s/%s", fn, entry->d_name);
            if (stat(path, &info) != 0)
            {
                LISSANDRA_LOG_ERROR("Error stat() en %s", path);
                return EXIT_FAILURE;
            }
            else
            {
                if (S_ISDIR(info.st_mode))
                {
                    int resultado = traverse(path, lista, entry->d_name);
                    if (resultado == EXIT_FAILURE)
                        return EXIT_FAILURE;
                }
                else if (S_ISREG(info.st_mode))
                {
                    if (strcmp(entry->d_name, "Metadata") != 0)
                        continue;

                    t_describe* desc = Malloc(sizeof(t_describe));
                    if (!get_table_metadata(tabla, desc))
                    {
                        Free(desc);
                        continue;
                    }

                    list_add(lista, desc);
                    break;
                }
            }
        }
    }

    closedir(dir);
    return EXIT_SUCCESS;
}

bool dirIsEmpty(char const* path)
{
    int n = 0;
    DIR* dir = opendir(path);
    if (!dir)
        return true;

    while (readdir(dir))
        if (++n > 2)
            break;

    closedir(dir);

    // Directorio vacio contiene solo 2 entradas: "." y ".."
    return n <= 2;
}

bool isLFSFile(char const* nombreArchivo)
{
    bool res = false;

    Vector strings = string_split(nombreArchivo, ".");
    if (!Vector_empty(&strings))
    {
        char const** ext = Vector_back(&strings);

        // tmpc || tmp || bin
        if (0 == strcmp(*ext, "tmpc") || 0 == strcmp(*ext, "tmp") || 0 == strcmp(*ext, "bin"))
            res = true;
    }
    Vector_Destruct(&strings);
    return res;
}

void generarPathArchivo(char const* nombreTabla, char const* nombreArchivo, char* buf)
{
    snprintf(buf, PATH_MAX, "%sTables/%s/%s", confLFS.PUNTO_MONTAJE, nombreTabla, nombreArchivo);
}

int traverse_to_drop(char const* pathTabla)
{
    DIR* dir = opendir(pathTabla);
    if (!dir)
    {
        LISSANDRA_LOG_ERROR("ERROR: La ruta %s especificada es invalida!", pathTabla);
        return -1;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)))
    {
        if (*entry->d_name != '.')
        {
            char pathArchivo[PATH_MAX];
            snprintf(pathArchivo, PATH_MAX, "%s/%s", pathTabla, entry->d_name);

            if (isLFSFile(entry->d_name))
            {
                borrarArchivoLFS(pathArchivo);
                continue;
            }

            unlink(pathArchivo);
        }
    }

    closedir(dir);
    return 0;
}

uint16_t get_particion(uint16_t particiones, uint16_t key)
{
    //key mod particiones de la tabla
    return key % particiones;
}

void generarPathParticion(uint16_t particion, char* pathTabla, char* pathParticion)
{
    snprintf(pathParticion, PATH_MAX, "%s/%d.bin", pathTabla, particion);
}

bool get_biggest_timestamp(char const* contenido, uint16_t key, t_registro* resultado)
{
    Vector registros = string_split(contenido, "\n");

    bool found = false;
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

        if (key == keyRegistro)
        {
            char* const value = fields[2];
            if (!found)
            {
                found = true;

                resultado->key = keyRegistro;
                resultado->timestamp = timestamp;
                strncpy(resultado->value, value, confLFS.TAMANIO_VALUE + 1);
            }
            else if (resultado->timestamp < timestamp)
            {
                resultado->timestamp = timestamp;
                strncpy(resultado->value, value, confLFS.TAMANIO_VALUE + 1);
            }
        }

        Vector_Destruct(&campos);
    }

    Vector_Destruct(&registros);
    return found;
}

bool scanParticion(char const* pathParticion, uint16_t key, t_registro* registro)
{
    char* contenido = leerArchivoLFS(pathParticion);
    if (!contenido)
        return NULL;

    bool resultado = get_biggest_timestamp(contenido, key, registro);
    Free(contenido);
    return resultado;
}

bool temporales_get_biggest_timestamp(char const* pathTabla, uint16_t key, t_registro* registro)
{
    DIR* dir;
    if (!(dir = opendir(pathTabla)))
    {
        LISSANDRA_LOG_FATAL("SELECT: No pude abrir directorio %s. No debería pasar!", pathTabla);
        return NULL;
    }

    bool foundAny = false;
    struct dirent* entry;
    while ((entry = readdir(dir)))
    {
        char path[PATH_MAX];
        if (*entry->d_name != '.')
        {
            struct stat info;
            snprintf(path, PATH_MAX, "%s/%s", pathTabla, entry->d_name);

            if (stat(path, &info) != 0)
            {
                LISSANDRA_LOG_SYSERROR("stat");
                closedir(dir);
                return NULL;
            }

            bool istmp = false;
            {
                Vector aux = string_split(path, ".");
                if (!Vector_empty(&aux))
                {
                    char const** ext = Vector_back(&aux);
                    if (0 == strcmp(*ext, "tmp") || 0 == strcmp(*ext, "tmpc"))
                        istmp = true;
                }
                Vector_Destruct(&aux);
            }

            if (!istmp)
                continue;

            char* contenido = leerArchivoLFS(path);
            if (!contenido)
                continue;

            t_registro* registroTemp = Malloc(REGISTRO_SIZE);
            if (!get_biggest_timestamp(contenido, key, registroTemp))
            {
                Free(registroTemp);
                Free(contenido);
                continue;
            }

            if (!foundAny)
            {
                foundAny = true;

                memcpy(registro, registroTemp, REGISTRO_SIZE);
            }
            else if (registro->timestamp < registroTemp->timestamp)
            {
                registro->timestamp = registroTemp->timestamp;
                strncpy(registro->value, registroTemp->value, confLFS.TAMANIO_VALUE + 1);
            }

            Free(registroTemp);
            Free(contenido);
        }
    }

    closedir(dir);
    return foundAny;
}

t_registro const* get_newest(t_registro const* particion, t_registro const* temporales, t_registro const* memtable)
{
    size_t num = 0;
    t_registro const* registros[3];

    if (particion)
        registros[num++] = particion;

    if (temporales)
        registros[num++] = temporales;

    if (memtable)
        registros[num++] = memtable;

    t_registro const* mayor = NULL;
    uint64_t timestampMayor = 0;
    for (size_t i = 0; i < num; ++i)
    {
        if (!mayor || registros[i]->timestamp > timestampMayor)
        {
            timestampMayor = registros[i]->timestamp;
            mayor = registros[i];
        }
    }

    return mayor;
}

char* leerArchivoLFS(const char* path)
{
    Vector bloques;
    size_t bytesLeft;
    {
        t_config* file = config_create(path);
        if (!file)
        {
            LISSANDRA_LOG_ERROR("No se encontro el archivo en el File System");
            return NULL;
        }

        bloques = config_get_array_value(file, "BLOCKS");
        bytesLeft = config_get_long_value(file, "SIZE");
        config_destroy(file);
    }

    size_t const longitudArchivo = bytesLeft;

    size_t const bloquesTotales = Vector_size(&bloques);

    // cuanto leer
    size_t readLen = confLFS.TAMANIO_BLOQUES;
    if (longitudArchivo < readLen)
        readLen = longitudArchivo;

    char* const contenido = Malloc(longitudArchivo + 1);
    size_t offset = 0;

    char** const arrayBloques = Vector_data(&bloques);
    for (size_t i = 0; i < bloquesTotales; ++i)
    {
        size_t const numBloque = strtoul(arrayBloques[i], NULL, 10);

        char pathBloque[PATH_MAX];
        generarPathBloque(numBloque, pathBloque);

        int fd = open(pathBloque, O_RDONLY);
        if (fd == -1)
        {
            LISSANDRA_LOG_SYSERROR("open");
            exit(EXIT_FAILURE);
        }

        char* mapping = mmap(0, confLFS.TAMANIO_BLOQUES, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapping == MAP_FAILED)
        {
            LISSANDRA_LOG_SYSERROR("mmap");
            exit(EXIT_FAILURE);
        }

        memcpy(contenido + offset, mapping, readLen);
        offset += readLen;
        bytesLeft -= readLen;

        if (munmap(mapping, confLFS.TAMANIO_BLOQUES) == -1)
        {
            LISSANDRA_LOG_SYSERROR("munmap");
            exit(EXIT_FAILURE);
        }

        close(fd);

        readLen = confLFS.TAMANIO_BLOQUES;
        if (bytesLeft < readLen)
            readLen = bytesLeft;
    }

    Vector_Destruct(&bloques);

    contenido[longitudArchivo] = '\0';
    return contenido;
}

static bool _pedirBloquesNuevos(char const* path, Vector* bloques, size_t n)
{
    Vector nuevos;
    Vector_Construct(&nuevos, sizeof(size_t), NULL, n);
    for (size_t i = 0; i < n; ++i)
    {
        size_t bloque;
        if (!buscarBloqueLibre(&bloque))
        {
            LISSANDRA_LOG_ERROR("No hay más bloques disponibles para guardar el archivo %s! Se perderán datos", path);

            // liberar los que pude pedir hasta ahora
            for (size_t j = 0; j < Vector_size(&nuevos); ++j)
            {
                size_t* const b = Vector_at(&nuevos, j);
                escribirValorBitarray(false, *b);
            }

            Vector_Destruct(&nuevos);
            return false;
        }

        Vector_push_back(&nuevos, &bloque);
    }

    for (size_t i = 0; i < Vector_size(&nuevos); ++i)
    {
        size_t* const bloque = Vector_at(&nuevos, i);

        char* blockNum = string_from_format("%u", *bloque);
        Vector_push_back(bloques, &blockNum);
    }

    Vector_Destruct(&nuevos);
    return true;
}

static inline void _escribirBloque(size_t block, char const* buf, size_t len)
{
    char pathBloque[PATH_MAX];
    generarPathBloque(block, pathBloque);

    int fd = open(pathBloque, O_RDWR);
    if (fd == -1)
    {
        LISSANDRA_LOG_SYSERROR("open");
        exit(EXIT_FAILURE);
    }

    char* mapping = mmap(NULL, confLFS.TAMANIO_BLOQUES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED)
    {
        LISSANDRA_LOG_SYSERROR("mmap");
        exit(EXIT_FAILURE);
    }

    memcpy(mapping, buf, len);

    munmap(mapping, confLFS.TAMANIO_BLOQUES);
    close(fd);
}

void escribirArchivoLFS(char const* path, char const* buf, size_t len)
{
    t_config* file = config_create(path);
    if (!file)
        return;

    size_t bloquesTotales = len / confLFS.TAMANIO_BLOQUES;
    if (len % confLFS.TAMANIO_BLOQUES)
        ++bloquesTotales;

    Vector blocks = config_get_array_value(file, "BLOCKS");

    size_t const bloquesActuales = Vector_size(&blocks);
    if (bloquesActuales < bloquesTotales)
    {
        // calcular cuantos bloques nuevos se requieren
        size_t const bloquesNecesarios = bloquesTotales - bloquesActuales;

        // pedir bloques nuevos
        if (!_pedirBloquesNuevos(path, &blocks, bloquesNecesarios))
        {
            Vector_Destruct(&blocks);
            config_destroy(file);
            return;
        }
    }

    char* newSize = string_from_format("%u", len);

    // listo, ya el archivo tiene los bloques suficientes, escribamos bloque a bloque
    char** const blockArray = Vector_data(&blocks);
    size_t i = 0;
    while (i < len / confLFS.TAMANIO_BLOQUES)
    {
        size_t const blockNum = strtoul(blockArray[i++], NULL, 10);

        _escribirBloque(blockNum, buf, confLFS.TAMANIO_BLOQUES);
        buf += confLFS.TAMANIO_BLOQUES;
    }

    // ultimo bloque
    if (len % confLFS.TAMANIO_BLOQUES)
        _escribirBloque(strtoul(blockArray[i], NULL, 10), buf, len % confLFS.TAMANIO_BLOQUES);

    config_set_value(file, "SIZE", newSize);
    Free(newSize);

    config_set_array_value(file, "BLOCKS", &blocks);
    Vector_Destruct(&blocks);

    config_save(file);
    config_destroy(file);

    LISSANDRA_LOG_TRACE("FS: Escribo %u bytes en archivo %s!", len, path);
}

void crearArchivoLFS(char const* path, size_t block)
{
    FILE* temporal = fopen(path, "w");
    fprintf(temporal, "SIZE=0\n");
    fprintf(temporal, "BLOCKS=[%u]\n", block);
    fclose(temporal);

    LISSANDRA_LOG_TRACE("FS: Creado archivo %s!", path);
}

void borrarArchivoLFS(char const* pathArchivo)
{
    Vector bloques;
    {
        t_config* data = config_create(pathArchivo);
        if (!data)
            return;

        bloques = config_get_array_value(data, "BLOCKS");
        config_destroy(data);
    }

    size_t const bloquesTotales = Vector_size(&bloques);

    char** const arrayBloques = Vector_data(&bloques);
    for (size_t i = 0; i < bloquesTotales; ++i)
    {
        size_t const numBloque = strtoul(arrayBloques[i], NULL, 10);

        // marco el bloque como libre
        escribirValorBitarray(false, numBloque);
    }

    unlink(pathArchivo);
    Vector_Destruct(&bloques);

    LISSANDRA_LOG_TRACE("FS: Borrado archivo %s!", pathArchivo);
}
