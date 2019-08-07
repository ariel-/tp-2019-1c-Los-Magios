
#ifndef Malloc_h__
#define Malloc_h__

#include <stddef.h>

void* Malloc(size_t size);
void* Calloc(size_t n, size_t elemSize);
void* Realloc(void* ptr, size_t newSize);
void Free(void* ptr);

#endif //Malloc_h__
