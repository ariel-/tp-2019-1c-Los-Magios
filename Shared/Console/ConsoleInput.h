
#ifndef ConsoleInput_h__
#define ConsoleInput_h__

#include "Logger.h"
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

static inline bool ValidateKey(char const* keyString, uint16_t* result)
{
    errno = 0;
    uint32_t k = strtoul(keyString, NULL, 10);
    if (errno || k > UINT16_MAX)
    {
        LISSANDRA_LOG_ERROR("Key %s invalida", keyString);
        return false;
    }

    *result = (uint16_t) k;
    return true;
}

static inline bool ValidateTableName(char const* const tableName)
{
    size_t len = 0;
    for (char const* itr = tableName; *itr; ++itr)
    {
        ++len;

        if (!isalnum(*itr))
        {
            LISSANDRA_LOG_ERROR("Tabla %s invalida", tableName);
            return false;
        }
    }

    if (len > NAME_MAX)
    {
        LISSANDRA_LOG_ERROR("La tabla %s tiene un nombre muy largo!", tableName);
        return false;
    }

    return true;
}

#endif //ConsoleInput_h__
