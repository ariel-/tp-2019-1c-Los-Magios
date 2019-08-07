
#ifndef Consistency_h__
#define Consistency_h__

#include "Logger.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum
{
    CRITERIA_SC,
    CRITERIA_SHC,
    CRITERIA_EC,

    NUM_CRITERIA,

    // valor especial para especificar que se debe enviar a cualquiera de las memorias
    CRITERIA_ANY = NUM_CRITERIA
} CriteriaType;

struct CriteriaString
{
    char const* String;
    CriteriaType Criteria;
};

static struct CriteriaString const CriteriaString[NUM_CRITERIA] =
{
    { "SC",  CRITERIA_SC },
    { "SHC", CRITERIA_SHC },
    { "EC",  CRITERIA_EC }
};

static inline bool CriteriaFromString(char const* string, CriteriaType* ct)
{
    for (uint8_t i = 0; i < NUM_CRITERIA; ++i)
    {
        if (!strcmp(string, CriteriaString[i].String))
        {
            *ct = CriteriaString[i].Criteria;
            return true;
        }
    }

    LISSANDRA_LOG_ERROR("Criterio %s no válido. Criterios validos: SC - SHC - EC.", string);
    return false;
}

#endif //Consistency_h__
