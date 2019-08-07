
#ifndef Frame_h__
#define Frame_h__

#include <stdint.h>
#include <time.h>

// Una frame es la página en memoria (registro)
#pragma pack(push, 1)
typedef struct
{
    uint64_t Timestamp;
    uint16_t Key;
    char Value[]; // tamaño maximo seteado en tiempo de ejecucion!
} Frame;
#pragma pack(pop)

typedef struct
{
    char const* TableName;
    Frame const* Frame;
} DirtyFrame;

#endif //Frame_h__
