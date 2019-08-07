
#include "Malloc.h"
#include "Logger.h"
#include <stdio.h>
#include <stdlib.h>

void* Malloc(size_t sz)
{
    void* ptr = malloc(sz);
    if (!ptr)
    {
        fputs("OOM: malloc() devolvió NULL", stderr);
        exit(1);
    }

    return ptr;
}

void* Calloc(size_t n, size_t elemSize)
{
    void* ptr = calloc(n, elemSize);
    if (!ptr)
    {
        fputs("OOM: calloc() devolvió NULL", stderr);
        exit(1);
    }

    return ptr;
}

void* Realloc(void* ptr, size_t newSize)
{
    void* ret = realloc(ptr, newSize);
    if (!ret)
    {
        fputs("OOM: realloc() devolvió NULL", stderr);
        exit(1);
    }

    return ret;
}

void Free(void* ptr)
{
    free(ptr);
}
