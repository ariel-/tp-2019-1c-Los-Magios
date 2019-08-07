
#include "Config.h"
#include "Commands.h"
#include "Criteria.h"
#include "Metadata.h"
#include "Runner.h"
#include <Appender.h>
#include <AppenderConsole.h>
#include <AppenderFile.h>
#include <Console.h>
#include <Defines.h>
#include <EventDispatcher.h>
#include <FileWatcher.h>
#include <libcommons/config.h>
#include <libcommons/string.h>
#include <Logger.h>
#include <Opcodes.h>
#include <Packet.h>
#include <pthread.h>
#include <Socket.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <Timer.h>

char CLIPrompt[] = "KRNL_LISSANDRA> ";

atomic_bool ProcessRunning = true;

KernelConfig ConfigKernel = { 0 };

static Appender* consoleLog = NULL;
static Appender* fileLog = NULL;

static PeriodicTimer* DescribeTimer = NULL;
static void PeriodicDescribe(PeriodicTimer*);

static PeriodicTimer* DiscoverTimer = NULL;

static void IniciarLogger(void)
{
    Logger_Init();

    AppenderFlags const loggerFlags = APPENDER_FLAGS_PREFIX_TIMESTAMP | APPENDER_FLAGS_PREFIX_LOGLEVEL;
    consoleLog = AppenderConsole_Create(LOG_LEVEL_INFO, loggerFlags, LMAGENTA, LCYAN, WHITE, YELLOW, LRED, RED);
    Logger_AddAppender(consoleLog);

    fileLog = AppenderFile_Create(LOG_LEVEL_TRACE, loggerFlags, "kernel.log", "w", 0);
    Logger_AddAppender(fileLog);
}

static void IniciarDispatch(void)
{
    if (!EventDispatcher_Init())
        exit(EXIT_FAILURE);
}

static void _loadReloadableFields(t_config const* config)
{
    // solo los campos recargables en tiempo ejecucion
    ConfigKernel.METADATA_REFRESH = config_get_long_value(config, "METADATA_REFRESH");
    atomic_store(&ConfigKernel.SLEEP_EJECUCION, config_get_long_value(config, "SLEEP_EJECUCION"));
    atomic_store(&ConfigKernel.QUANTUM, config_get_long_value(config, "QUANTUM"));
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

    // recargo el intervalo de metadata
    PeriodicTimer_ReSetTimer(DescribeTimer, ConfigKernel.METADATA_REFRESH);
}

static void SetupConfigInitial(char const* fileName)
{
    static uint32_t const METRICS_INTERVAL = 30 * 1000;
    static uint32_t const DISCOVERY_INTERVAL = 10 * 1000;

    LISSANDRA_LOG_INFO("Cargando archivo de configuracion %s...", fileName);

    t_config* config = config_create(fileName);
    if (!config)
    {
        LISSANDRA_LOG_FATAL("No se pudo abrir archivo de configuracion %s!", fileName);
        exit(EXIT_FAILURE);
    }

    snprintf(ConfigKernel.IP_MEMORIA, INET6_ADDRSTRLEN, "%s", config_get_string_value(config, "IP_MEMORIA"));
    snprintf(ConfigKernel.PUERTO_MEMORIA, PORT_STRLEN, "%s", config_get_string_value(config, "PUERTO_MEMORIA"));
    ConfigKernel.MULTIPROCESAMIENTO = config_get_long_value(config, "MULTIPROCESAMIENTO");

    _loadReloadableFields(config);

    config_destroy(config);

    // notificarme si hay cambios en la config
    FileWatcher* fw = FileWatcher_Create();
    FileWatcher_AddWatch(fw, fileName, _reLoadConfig);
    EventDispatcher_AddFDI(fw);

    // timers
    DescribeTimer = PeriodicTimer_Create(ConfigKernel.METADATA_REFRESH, PeriodicDescribe);
    EventDispatcher_AddFDI(DescribeTimer);

    // cada 30 segundos debe mostrar las metricas por consola
    EventDispatcher_AddFDI(PeriodicTimer_Create(METRICS_INTERVAL, Criterias_Report));

    // cada 10 segundos? no dice nada el enunciado asi que pongo arbitrariamente el intervalo
    // en fin, cada 10 segundos dije! se hace el discovery de memorias aka gossiping
    DiscoverTimer = PeriodicTimer_Create(DISCOVERY_INTERVAL, Criterias_Update);
    EventDispatcher_AddFDI(DiscoverTimer);
}

static void InitMemorySubsystem(void)
{
    Criterias_Init(DiscoverTimer);
    Metadata_Init();
}

static void MainLoop(void)
{
    // Runner cambia el manejador de consola asi que ejecuto antes de abrir la consola
    Runner_Init();

    // el kokoro
    pthread_t consoleTid;
    pthread_create(&consoleTid, NULL, CliThread, NULL);

    EventDispatcher_Loop();

    pthread_join(consoleTid, NULL);
}

static void Cleanup(void)
{
    Metadata_Destroy();
    Criterias_Destroy();
    EventDispatcher_Terminate();
    Logger_Terminate();
}

int main(void)
{
    static char const configFileName[] = "kernel.conf";

    IniciarLogger();
    IniciarDispatch();
    SigintSetup();
    SetupConfigInitial(configFileName);

    InitMemorySubsystem();

    MainLoop();

    Cleanup();
}

static void PeriodicDescribe(PeriodicTimer* pt)
{
    (void) pt;

    // hago un describe global
    Vector args = string_n_split("DESCRIBE", 1, NULL);

    HandleDescribe(&args);

    Vector_Destruct(&args);
}
