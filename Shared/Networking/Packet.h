#ifndef Packet_h__
#define Packet_h__

#include "ByteConverter.h"
#include "Malloc.h"
#include "vector.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define declare_append_func(type) \
static inline void Packet_Append_##type(Packet* p, type val)

declare_append_func(char);
declare_append_func(int8_t);
declare_append_func(uint8_t);
declare_append_func(int16_t);
declare_append_func(uint16_t);
declare_append_func(int32_t);
declare_append_func(uint32_t);
declare_append_func(int64_t);
declare_append_func(uint64_t);
declare_append_func(bool);
static inline void Packet_Append_str(Packet* p, char const* str);

#undef declare_append_func

#define declare_read_func(type) \
static inline void Packet_Read_##type(Packet* p, type* res)

declare_read_func(char);
declare_read_func(int8_t);
declare_read_func(uint8_t);
declare_read_func(int16_t);
declare_read_func(uint16_t);
declare_read_func(int32_t);
declare_read_func(uint32_t);
declare_read_func(int64_t);
declare_read_func(uint64_t);
declare_read_func(bool);

static inline void Packet_Read_str(Packet* p, char** res);

#undef declare_read_func

typedef struct Packet
{
    size_t rpos, wpos;
    Vector data;
    uint16_t cmd;
} Packet;

/*
 * Packet_Create: inicializa un paquete
 * p: Paquete a inicializar
 * cmd: opcode del paquete, debería ser parte del enum Opcodes
 * res: reservar X bytes para el buffer de datos, puede ser 0, en cuyo caso el buffer se expandirá al añadir nuevos datos
 */
static inline Packet* Packet_Create(uint16_t cmd, uint16_t res)
{
    Packet* p = Malloc(sizeof(Packet));
    p->rpos = 0;
    p->wpos = 0;
    p->cmd = cmd;
    Vector_Construct(&p->data, sizeof(uint8_t), NULL, res);

    return p;
}

/*
 * Packet_Adopt: uso interno de la API de Socket, sirve para cambiar contenidos con un buffer
 * de forma tal que podemos utilizar los datos ya leídos sin necesidad de copiarlos
 */
static inline Packet* Packet_Adopt(uint16_t cmd, uint8_t* buf, size_t bufSize)
{
    Packet* p = Malloc(sizeof(Packet));
    p->rpos = 0;
    p->wpos = 0;
    p->cmd = cmd;
    Vector_adopt(&p->data, buf, bufSize);

    return p;
}

/*
 * Packet_GetOpcode: devuelve el opcode del paquete
 */
static inline uint16_t Packet_GetOpcode(Packet const* p)
{
    return p->cmd;
}

/*
 * Packet_Size: devuelve el tamaño en bytes de los datos almacenados en el buffer
 */
static inline size_t Packet_Size(Packet const* p)
{
    return Vector_size(&p->data);
}

/*
 * Packet_Contents: devuelve un puntero a la primera posición del buffer
 */
static inline uint8_t* Packet_Contents(Packet const* p)
{
    return Vector_data(&p->data);
}

// limpia el contenido del paquete
static inline void Packet_Clear(Packet* p)
{
    Vector_clear(&p->data);
    p->rpos = 0;
    p->wpos = 0;
}

/*
 * Packet_Append: inserta el valor X en el buffer
 */
#define Packet_Append(p, X) _Generic((X), \
       char: Packet_Append_char,          \
     int8_t: Packet_Append_int8_t,        \
    uint8_t: Packet_Append_uint8_t,       \
    int16_t: Packet_Append_int16_t,       \
   uint16_t: Packet_Append_uint16_t,      \
    int32_t: Packet_Append_int32_t,       \
   uint32_t: Packet_Append_uint32_t,      \
    int64_t: Packet_Append_int64_t,       \
   uint64_t: Packet_Append_uint64_t,      \
      char*: Packet_Append_str,           \
char const*: Packet_Append_str,           \
       bool: Packet_Append_bool           \
) ((p), (X))

/*
 * Packet_Read: lee un valor desde el buffer y lo guarda en la dirección apuntada por X
 */
#define Packet_Read(p, X) _Generic((X), \
      char*: Packet_Read_char,          \
    int8_t*: Packet_Read_int8_t,        \
   uint8_t*: Packet_Read_uint8_t,       \
   int16_t*: Packet_Read_int16_t,       \
  uint16_t*: Packet_Read_uint16_t,      \
   int32_t*: Packet_Read_int32_t,       \
  uint32_t*: Packet_Read_uint32_t,      \
   int64_t*: Packet_Read_int64_t,       \
  uint64_t*: Packet_Read_uint64_t,      \
     char**: Packet_Read_str,           \
      bool*: Packet_Read_bool           \
) ((p), (X))

/*
 * Packet_Destroy: destruye el paquete
 */
static inline void Packet_Destroy(Packet* p)
{
    Vector_Destruct(&p->data);
    Free(p);
}

#define append_to_packet(src, len)                      \
do                                                      \
{                                                       \
    size_t const newSize = p->wpos + (len);             \
    if (Vector_capacity(&p->data) < newSize)            \
    {                                                   \
        if (newSize < 100)                              \
            Vector_reserve(&p->data, 300);              \
        else if (newSize < 750)                         \
            Vector_reserve(&p->data, 2500);             \
        else if (newSize < 6000)                        \
            Vector_reserve(&p->data, 10000);            \
        else                                            \
            Vector_reserve(&p->data, 400000);           \
    }                                                   \
                                                        \
    if (Vector_size(&p->data) < newSize)                \
        Vector_resize_zero(&p->data, newSize);          \
                                                        \
    memcpy(Vector_at(&p->data, p->wpos), (src), (len)); \
    p->wpos = newSize;                                  \
} while (false)

#define read_from_packet(dst, len)                      \
do                                                      \
{                                                       \
    memcpy((dst), Vector_at(&p->data, p->rpos), (len)); \
    p->rpos += (len);                                   \
} while (false)

#define define_append_numeric_value(type)                    \
static inline void Packet_Append_##type(Packet* p, type val) \
{                                                            \
    val = EndianConvert(val);                                \
    append_to_packet(&val, sizeof(type));                    \
}

define_append_numeric_value(char)
define_append_numeric_value(int8_t)
define_append_numeric_value(uint8_t)
define_append_numeric_value(int16_t)
define_append_numeric_value(uint16_t)
define_append_numeric_value(int32_t)
define_append_numeric_value(uint32_t)
define_append_numeric_value(int64_t)
define_append_numeric_value(uint64_t)

#undef define_append_numeric_value

static inline void Packet_Append_str(Packet* p, char const* str)
{
    uint32_t len = str ? strlen(str) : 0;
    Packet_Append(p, len);
    if (len)
        append_to_packet(str, len);
}

static inline void Packet_Append_bool(Packet* p, bool val)
{
    // serialize as char
    char c = val ? 1 : 0;
    Packet_Append(p, c);
}

#define define_read_numeric_value(type)                        \
static inline void Packet_Read_##type(Packet* p, type* res)    \
{                                                              \
    type val;                                                  \
    read_from_packet(&val, sizeof(type));                      \
    *res = EndianConvert(val);                                 \
}

define_read_numeric_value(char)
define_read_numeric_value(int8_t)
define_read_numeric_value(uint8_t)
define_read_numeric_value(int16_t)
define_read_numeric_value(uint16_t)
define_read_numeric_value(int32_t)
define_read_numeric_value(uint32_t)
define_read_numeric_value(int64_t)
define_read_numeric_value(uint64_t)

#undef define_read_numeric_value

static inline void Packet_Read_str(Packet* p, char** res)
{
    uint32_t strLen;
    Packet_Read(p, &strLen);

    char* str = Malloc(strLen + 1);
    read_from_packet(str, strLen);
    str[strLen] = '\0';
    *res = str;
}

static inline void Packet_Read_bool(Packet* p, bool* res)
{
    // serialize as char
    char c;
    Packet_Read(p, &c);
    *res = (c != 0);
}

#undef read_from_packet
#undef append_to_packet

#endif //Packet_h__
