
#ifndef LISSANDRA_FILESYSTEM_H
#define LISSANDRA_FILESYSTEM_H

#include <libcommons/bitarray.h>

/**
 * Variables
 */

extern char pathMetadataBitarray[];

extern t_bitarray* bitArray;

//typedef uint32_t t_num;

//Semaforo

//pthread_mutex_t solicitud_mutex;

//Funciones

void iniciarFileSystem(void);

void terminarFileSystem(void);

#endif //LISSANDRA_FILESYSTEM_H
