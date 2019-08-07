
#ifndef Metadata_h__
#define Metadata_h__

#include "Criteria.h"
#include <stdbool.h>

void Metadata_Init(void);

void Metadata_Add(char const* name, CriteriaType ct);

bool Metadata_Get(char const* name, CriteriaType* ct);

void Metadata_Update(char const* tableName, Packet* p);

void Metadata_Destroy(void);

#endif //Metadata_h__
