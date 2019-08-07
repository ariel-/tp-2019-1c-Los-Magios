
#ifndef FileWatcher_h__
#define FileWatcher_h__

#include "FileDescInterface.h"
#include <libcommons/hashmap.h>

typedef struct
{
    FDI _impl;

    t_hashmap* WatchCallbacks;
} FileWatcher;

/*
 * Crea un nuevo FileWatcher, el mismo está configurado para ser no bloqueante y debe ser atendido por event loop
 */
FileWatcher* FileWatcher_Create(void);

/*
 * Setea el FileWatcher para que monitoree cambios en el archivo o directorio especificado
 * La función notifyFn es llamada al detectarse el cambio,
 * la misma recibirá como parámetro el nombre de archivo modificado
 */
typedef void(*FWCallback)(char const* fileName);
bool FileWatcher_AddWatch(FileWatcher* fw, char const* pathName, FWCallback notifyFn);

/*
 * Destruye un FileWatcher
 */
void FileWatcher_Destroy(void* elem);

#endif //FileWatcher_h__
