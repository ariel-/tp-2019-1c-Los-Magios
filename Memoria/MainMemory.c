
#include "MainMemory.h"
#include "API.h"
#include "Config.h"
#include "PageTable.h"
#include "SegmentTable.h"
#include <libcommons/bitarray.h>
#include <Logger.h>
#include <Malloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <Timer.h>

// la memoria es un arreglo de marcos contiguos
static Frame* Memory = NULL;
static uint32_t MaxValueLength = 0;
static size_t FrameSize = 0;
static size_t NumFrames = 0;

// bitmap de frames libres
static uint8_t* FrameBitmap = NULL;
static t_bitarray* FrameStatus = NULL;
static bool Full = false; // valor cacheado para no tener que pasar por LRU de nuevo

static void _cleanMemory(void);
static PageTable* CreateNewPage(size_t frameNumber, char const* tableName, uint16_t key);
static void WriteFrame(size_t frameNumber, uint64_t timestamp, uint16_t key, char const* value);
static bool GetFreeFrame(size_t* frame);

void Memory_Initialize(uint32_t maxValueLength, char const* mountPoint)
{
    // malloc de n bytes contiguos
    size_t const allocSize = ConfigMemoria.TAM_MEM;

    // el valor se guarda contiguo junto a los otros datos de longitud fija
    // si es menor al maximo se halla null terminated de lo contrario no
    FrameSize = sizeof(Frame) + maxValueLength;
    NumFrames = allocSize / FrameSize;

    if (!NumFrames)
    {
        LISSANDRA_LOG_FATAL("El valor de TAM_MEM es demasiado pequeño, asignar al menos %d bytes!", FrameSize);
        exit(EXIT_FAILURE);
    }

    Memory = Malloc(allocSize);
    MaxValueLength = maxValueLength;

    size_t bitmapBytes = NumFrames / 8;
    if (NumFrames % 8)
        ++bitmapBytes;

    FrameBitmap = Calloc(bitmapBytes, 1);
    FrameStatus = bitarray_create_with_mode(FrameBitmap, bitmapBytes, MSB_FIRST);

    SegmentTable_Initialize(mountPoint);

    char const* const frameString = NumFrames == 1 ? "marco" : "marcos";
    LISSANDRA_LOG_INFO("Memoria inicializada. Tamaño: %d bytes (%d %s). Tamaño de marco: %d", allocSize, NumFrames,
                       frameString, FrameSize);
}

bool Memory_InsertNewValue(char const* tableName, uint64_t timestamp, uint16_t key, char const* value)
{
    size_t freeFrame;
    if (!GetFreeFrame(&freeFrame))
        return false;

    CreateNewPage(freeFrame, tableName, key);
    WriteFrame(freeFrame, timestamp, key, value);
    return true;
}

bool Memory_UpsertValue(char const* tableName, uint16_t key, char const* value)
{
    PageTable* pt = SegmentTable_GetPageTable(tableName);
    size_t frame;
    if (!pt || !PageTable_GetFrameNumber(pt, key, &frame))
    {
        if (!GetFreeFrame(&frame))
            return false;

        pt = CreateNewPage(frame, tableName, key);
    }

    WriteFrame(frame, GetMSEpoch(), key, value);
    PageTable_MarkDirty(pt, key);

    LISSANDRA_LOG_TRACE("Memoria: marcada page %hu como modificada. (Frame: %u)", key, frame);
    return true;
}

void Memory_CleanFrame(size_t frameNumber)
{
    bitarray_clean_bit(FrameStatus, frameNumber);
}

void Memory_EvictPages(char const* tableName)
{
    SegmentTable_DeleteSegment(tableName);
}

Frame* Memory_GetFrame(char const* tableName, uint16_t key)
{
    PageTable* pt = SegmentTable_GetPageTable(tableName);
    if (!pt)
        return NULL;

    size_t frame;
    if (!PageTable_GetFrameNumber(pt, key, &frame))
        return NULL;

    return Memory_Read(frame);
}

uint32_t Memory_GetMaxValueLength(void)
{
    return MaxValueLength;
}

Frame* Memory_Read(size_t frameNumber)
{
    return (Frame*) ((uint8_t*) Memory + frameNumber * FrameSize);
}

void Memory_DoJournal(void(*insertFn)(void*))
{
    Vector v;
    Vector_Construct(&v, sizeof(DirtyFrame), NULL, 0);
    SegmentTable_GetDirtyFrames(&v);

    Vector_iterate(&v, insertFn);

    // limpiar memoria
    _cleanMemory();

    Vector_Destruct(&v);
}

void Memory_Destroy(void)
{
    SegmentTable_Destroy();
    bitarray_destroy(FrameStatus);
    Free(FrameBitmap);
    Free(Memory);
}

/* PRIVATE */
static void _cleanMemory(void)
{
    memset(FrameBitmap, 0, bitarray_get_max_bit(FrameStatus) / 8);
    SegmentTable_Clean();
    Full = false;

    LISSANDRA_LOG_TRACE("Memoria: limpiadas estructuras");
}

static PageTable* CreateNewPage(size_t frameNumber, char const* tableName, uint16_t key)
{
    PageTable* pt = SegmentTable_GetPageTable(tableName);
    if (!pt)
        pt = SegmentTable_CreateSegment(tableName);

    PageTable_AddPage(pt, key, frameNumber);
    return pt;
}

static void WriteFrame(size_t frameNumber, uint64_t timestamp, uint16_t key, char const* value)
{
    Frame* const f = Memory_Read(frameNumber);

    f->Key = key;
    f->Timestamp = timestamp;
    strncpy(f->Value, value, MaxValueLength);

    LISSANDRA_LOG_TRACE("Memoria: escrito frame %p (offset: %u, timestamp: %" PRIu64
                                ", key: %hu, value: '%s'", (void*) f, frameNumber, timestamp, key, value);
}

static bool GetFreeFrame(size_t* frame)
{
    if (Full)
        return false;

    size_t i = 0;
    for (; i < NumFrames; ++i)
        if (!bitarray_test_bit(FrameStatus, i))
            break;

    if (i == NumFrames)
    {
        LISSANDRA_LOG_TRACE("Memoria: no hay marcos libres, iniciando LRU");

        // no encontre ninguno libre, desalojar el LRU
        size_t freeFrame;
        if (!SegmentTable_GetLRUFrame(&freeFrame))
        {
            LISSANDRA_LOG_TRACE("Memoria: estado FULL");
            Full = true;
            return false;
        }

        LISSANDRA_LOG_TRACE("Memoria: desalojado marco %u", freeFrame);
        i = freeFrame;
    }
    else
        LISSANDRA_LOG_TRACE("Memoria: asignado marco %u", i);

    bitarray_set_bit(FrameStatus, i);
    *frame = i;
    return true;
}
