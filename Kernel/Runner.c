
#include "Runner.h"
#include "Commands.h"
#include "Config.h"
#include <Console.h>
#include <libcommons/string.h>
#include <libcommons/queue.h>
#include <Logger.h>
#include <Malloc.h>
#include <string.h>
#include <Threads.h>
#include <Timer.h>

typedef struct
{
    char ScriptName[NAME_MAX + 1];
    Vector Data;
    size_t IP;
} TCB;

static TCB* _create_task(char const* scriptName, Vector const* data);
static void _delete_task(TCB* tcb);

static void _addSingleLine(char const* line);

static void _addToReadyQueue(TCB* tcb);
static bool _parseCommand(char const* command);

static void* _workerThread(void*);

static t_queue* ReadyQueue;
static pthread_mutex_t QueueLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t QueueCond = PTHREAD_COND_INITIALIZER;

void Runner_Init(void)
{
    CommandParser = _addSingleLine;

    ReadyQueue = queue_create();
    for (uint32_t i = 0; i < ConfigKernel.MULTIPROCESAMIENTO; ++i)
        Threads_CreateDetached(_workerThread, (void*) (i + 1));
}

void Runner_AddScript(char const* scriptName, File* sc)
{
    Vector script = file_getlines(sc);
    file_close(sc);

    _addToReadyQueue(_create_task(scriptName, &script));
}

/* PRIVATE*/
static TCB* _create_task(char const* scriptName, Vector const* data)
{
    TCB* tcb = Malloc(sizeof(TCB));
    *tcb->ScriptName = '\0';
    strncat(tcb->ScriptName, scriptName, NAME_MAX);

    tcb->Data = *data;
    tcb->IP = 0;

    return tcb;
}

static void _delete_task(TCB* tcb)
{
    Vector_Destruct(&tcb->Data);
    Free(tcb);
}

static void _addSingleLine(char const* line)
{
    Vector script = string_n_split(line, 1, NULL);
    _addToReadyQueue(_create_task("unitario", &script));
}

static void _addToReadyQueue(TCB* tcb)
{
    pthread_mutex_lock(&QueueLock);
    queue_push(ReadyQueue, tcb);
    pthread_cond_signal(&QueueCond); // signal a un worker
    pthread_mutex_unlock(&QueueLock);
}

static bool _parseCommand(char const* command)
{
    // copy pasteo horrible de Console.c pero no se me ocurre como evitar la duplicacion codigo
    // ya que LFS y Memoria van a usar los handler de consola directamente, sin pasar por un runner como en este caso
    size_t spc = strcspn(command, " ");
    char cmd[spc + 1];
    strncpy(cmd, command, spc + 1);
    cmd[spc] = '\0';

    // por defecto falso, ejemplo el comando no es uno valido
    bool res = false;
    Vector args = string_q_split(command, ' ');
    for (uint32_t i = 0; ScriptCommands[i].CmdName != NULL; ++i)
    {
        if (!strcmp(ScriptCommands[i].CmdName, cmd))
        {
            if (ScriptCommands[i].Handler(&args))
                res = true;
            break;
        }
    }

    Vector_Destruct(&args);
    return res;
}

static void* _workerThread(void* arg)
{
    uint32_t const execState = (uint32_t) arg;
    LISSANDRA_LOG_TRACE("RUNNER: Iniciando estado EXEC %u", execState);

    while (ProcessRunning)
    {
        TCB* tcb;
        pthread_mutex_lock(&QueueLock);
        while (!(tcb = queue_pop(ReadyQueue)))
            pthread_cond_wait(&QueueCond, &QueueLock);
        pthread_mutex_unlock(&QueueLock);

        // ahora tcb esta cargado con un script, proseguimos
        // campos recargables asi que consulto al atender una nueva tarea
        uint32_t q = atomic_load(&ConfigKernel.QUANTUM);
        uint32_t delayMS = atomic_load(&ConfigKernel.SLEEP_EJECUCION);

        bool abnormalTermination = false;
        uint32_t exec = 0;
        while (tcb->IP < Vector_size(&tcb->Data))
        {
            char* const* const lines = Vector_data(&tcb->Data);
            char* const IR = lines[tcb->IP++]; // Instruction Register :P

            bool res = _parseCommand(IR);
            MSSleep(delayMS);
            if (!res)
            {
                LISSANDRA_LOG_ERROR("RUNNER %u: error al ejecutar linea %u (\"%s\")", execState, tcb->IP, IR);
                abnormalTermination = true;
                break;
            }

            if (++exec == q)
                break;
        }

        // salimos por un error en ejecucion o termino el script?
        if (abnormalTermination || tcb->IP == Vector_size(&tcb->Data))
        {
            LISSANDRA_LOG_TRACE("RUNNER %u: script %s termino por %s. Script pasa a EXIT", execState, tcb->ScriptName, abnormalTermination ? "error en ejecucion" : "fin de lineas");

            // borrar los datos
            _delete_task(tcb);
            continue;
        }

        // solo termino el quantum pero quedan lineas por ejecutar, replanificarlas
        _addToReadyQueue(tcb);
        LISSANDRA_LOG_TRACE("RUNNER %u: desalojado script %s por fin de quantum. Script agregado a READY nuevamente", execState, tcb->ScriptName);
    }

    return NULL;
}
