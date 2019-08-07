
#ifndef Memoria_API_h
#define Memoria_API_h

#include "Frame.h"
#include <Consistency.h>
#include <Malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <vector.h>

typedef struct PeriodicTimer PeriodicTimer;

typedef struct
{
    char* tableName;
    uint8_t ct;
    uint16_t parts;
    uint32_t compTime;
} TableMD;

static inline void FreeMD(void* md)
{
    TableMD* const p = md;
    Free(p->tableName);
}

typedef enum
{
    Ok,
    KeyNotFound,
    TableNotFound,
    MemoryFull
} SelectResult;

// devuelve un código dependiendo del resultado
// value debe apuntar a un espacio con (maxValueLength+1) bytes
SelectResult API_Select(char const* tableName, uint16_t key, char* value);

typedef enum
{
    InsertOk,
    InsertOverflow,
    InsertFull
} InsertResult;

// en memoria va sin timestamp el request, se calcula luego. ver #1355
InsertResult API_Insert(char const* tableName, uint16_t key, char const* value);

// devuelve EXIT_FAILURE si la tabla ya existe en el FS
uint8_t API_Create(char const* tableName, CriteriaType ct, uint16_t partitions, uint32_t compactionTime);

// solicita al FS directamente
// results se encuentra construido con sizeof(struct MD)
// devuelve false si la tabla no existe en FS
bool API_Describe(char const* tableName, Vector* results);

// devuelve EXIT_FAILURE si la tabla no existe en el FS
uint8_t API_Drop(char const* tableName);

void API_Journal(PeriodicTimer* pt);

#endif //Memoria_API_h
