
// Macros para convertir de y hacia network-byte-order aka Big-Endian

#ifndef ByteConverter_h__
#define ByteConverter_h__

#include <endian.h>

#if BYTE_ORDER == BIG_ENDIAN
#define EndianConvert(val) val
#elif BYTE_ORDER == LITTLE_ENDIAN
#include <byteswap.h>
#include <stdint.h>

// _Generic solo acepta nombres de funciones, no macros, asi que tengo que agregar wrappers. Auch

static inline int16_t bswap16(int16_t x)
{
    return bswap_16(x);
}

static inline uint16_t bswap16u(uint16_t x)
{
    return bswap_16(x);
}

static inline int32_t bswap32(int32_t x)
{
    return bswap_32(x);
}

static inline uint32_t bswap32u(uint32_t x)
{
    return bswap_32(x);
}

static inline int64_t bswap64(int64_t x)
{
    return bswap_64(x);
}

static inline uint64_t bswap64u(uint64_t x)
{
    return bswap_64(x);
}

static inline char charid(char x)
{
    return x;
}

static inline uint8_t ucharid(uint8_t x)
{
    return x;
}

#define EndianConvert(val) _Generic((val), \
    int16_t: bswap16,                      \
   uint16_t: bswap16u,                     \
    int32_t: bswap32,                      \
   uint32_t: bswap32u,                     \
    int64_t: bswap64,                      \
   uint64_t: bswap64u,                     \
       char: charid,                       \
     int8_t: charid,                       \
    uint8_t: ucharid                       \
) (val)
#else
#error Endianness not supported!
#endif //BYTE_ORDER

#endif //ByteConverter_h__
