
#ifndef MainMemory_h__
#define MainMemory_h__

#include "Frame.h"
#include <stdbool.h>
#include <stdint.h>

void Memory_Initialize(uint32_t maxValueLength, char const* mountPoint);

bool Memory_InsertNewValue(char const* tableName, uint64_t timestamp, uint16_t key, char const* value);

bool Memory_UpsertValue(char const* tableName, uint16_t key, char const* value);

void Memory_CleanFrame(size_t frameNumber);

void Memory_EvictPages(char const* tableName);

Frame* Memory_GetFrame(char const* tableName, uint16_t key);

uint32_t Memory_GetMaxValueLength(void);

Frame* Memory_Read(size_t frameNumber);

void Memory_DoJournal(void(*insertFn)(void*));

void Memory_Destroy(void);

#endif //MainMemory_h__
