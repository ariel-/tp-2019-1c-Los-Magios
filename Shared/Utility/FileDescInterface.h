
#ifndef FileDescInterface_h__
#define FileDescInterface_h__

#include <stdbool.h>

typedef bool FDICallbackFn(void* fdi);
typedef void FDIDestroyFn(void* fdi);

typedef struct FileDescriptorInterface
{
    int Handle;

    FDICallbackFn* ReadCallback;

    FDIDestroyFn* _destroy;
} FDI;

static inline void FDI_Destroy(void* fdi)
{
    ((FDI*)fdi)->_destroy(fdi);
}

#endif //FileDescInterface_h__
