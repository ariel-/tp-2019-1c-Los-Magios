
#include "LissandraLibrary.h"
#include "API.h"
#include "CLIHandlers.h"
#include "Config.h"
#include "FileSystem.h"
#include <Appender.h>
#include <AppenderConsole.h>
#include <AppenderFile.h>
#include <EventDispatcher.h>
#include <FileWatcher.h>
#include <libcommons/config.h>
#include <libcommons/string.h>
#include <Logger.h>
#include <Malloc.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <Threads.h>

CLICommand const CLICommands[] =
{
    { "SELECT",   HandleSelect   },
    { "INSERT",   HandleInsert   },
    { "CREATE",   HandleCreate   },
    { "DESCRIBE", HandleDescribe },
    { "DROP",     HandleDrop     },
    { NULL,       NULL           }
};

char CLIPrompt[] = "FS_LISSANDRA> ";

atomic_bool ProcessRunning = true;

static Appender* consoleLog;
static Appender* fileLog;

static PeriodicTimer* DumpTimer = NULL;
static void _initDumpThread(PeriodicTimer* pt);

t_config_FS confLFS = { 0 };

static void IniciarLogger(void)
{
    Logger_Init();

    AppenderFlags const consoleFlags = APPENDER_FLAGS_PREFIX_TIMESTAMP | APPENDER_FLAGS_PREFIX_LOGLEVEL;
    //Cambiamos el color "198EDC por "EA899A""
    consoleLog = AppenderConsole_Create(LOG_LEVEL_INFO, consoleFlags, LMAGENTA, LRED, LRED, YELLOW, LGREEN, WHITE);
    Logger_AddAppender(consoleLog);

    AppenderFlags const fileFlags = consoleFlags;
    fileLog = AppenderFile_Create(LOG_LEVEL_TRACE, fileFlags, "lfs.log", "w", 0);
    Logger_AddAppender(fileLog);

}

static void _loadReloadableFields(t_config const* config)
{
    // solo los campos recargables en tiempo ejecucion
    atomic_store(&confLFS.RETARDO, config_get_long_value(config, "RETARDO"));
    confLFS.TIEMPO_DUMP = config_get_long_value(config, "TIEMPO_DUMP");
}

static void _reLoadConfig(char const* fileName)
{
    LISSANDRA_LOG_INFO("Configuracion modificada, recargando campos...");
    t_config* config = config_create(fileName);
    if (!config)
    {
        LISSANDRA_LOG_ERROR("No se pudo abrir archivo de configuracion %s!", fileName);
        return;
    }

    _loadReloadableFields(config);
    config_destroy(config);

    // recargo el intervalo de dumps
    PeriodicTimer_ReSetTimer(DumpTimer, confLFS.TIEMPO_DUMP);
}

static void LoadConfigInitial(char const* fileName)
{
    LISSANDRA_LOG_INFO("Cargando archivo de configuracion %s...", fileName);

    t_config* config = config_create(fileName);
    if (!config)
    {
        LISSANDRA_LOG_FATAL("No se pudo abrir archivo de configuracion %s!", fileName);
        exit(EXIT_FAILURE);
    }

    //Esto agregamos nosotras
    snprintf(confLFS.PUERTO_ESCUCHA, PORT_STRLEN, "%s", config_get_string_value(config, "PUERTO_ESCUCHA"));

    char const* mountPoint = config_get_string_value(config, "PUNTO_MONTAJE");
    if (!string_ends_with(mountPoint, "/"))
    {
        // agregar un '/' al final
        snprintf(confLFS.PUNTO_MONTAJE, PATH_MAX, "%s/", mountPoint);
    }
    else
        snprintf(confLFS.PUNTO_MONTAJE, PATH_MAX, "%s", mountPoint);

    confLFS.TAMANIO_VALUE = config_get_long_value(config, "TAMANIO_VALUE");
    confLFS.TAMANIO_BLOQUES = config_get_long_value(config, "BLOCK_SIZE");
    confLFS.CANTIDAD_BLOQUES = config_get_long_value(config, "BLOCKS");

    _loadReloadableFields(config);

    config_destroy(config);

    // notificarme si hay cambios en la config
    FileWatcher* fw = FileWatcher_Create();
    FileWatcher_AddWatch(fw, fileName, _reLoadConfig);
    EventDispatcher_AddFDI(fw);

    // timers
    DumpTimer = PeriodicTimer_Create(confLFS.TIEMPO_DUMP, _initDumpThread);
    EventDispatcher_AddFDI(DumpTimer);

    LISSANDRA_LOG_TRACE("Config LFS iniciado");
}

static void MainLoop(void)
{
    // el kokoro
    pthread_t consoleTid;
    pthread_create(&consoleTid, NULL, CliThread, NULL);

    EventDispatcher_Loop();

    pthread_join(consoleTid, NULL);
}

static void Cleanup(void)
{
    memtable_destroy();
    terminarFileSystem();
    EventDispatcher_Terminate();
    Logger_Terminate();
}

int main(void)
{
    static char const configFileName[] = "lissandra.conf";

    IniciarLogger();
    EventDispatcher_Init();
    SigintSetup();
    LoadConfigInitial(configFileName);

    iniciarFileSystem();
    memtable_create();

    iniciar_servidor();
    MainLoop();

    Cleanup();
}

static void _initDumpThread(PeriodicTimer* pt)
{
    PeriodicTimer_SetEnabled(pt, false);

    Threads_CreateDetached(memtable_dump_thread, pt);
}
