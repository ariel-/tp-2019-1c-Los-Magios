
#include "SegmentTable.h"
#include "MainMemory.h"
#include <libcommons/dictionary.h>
#include <linux/limits.h>
#include <Malloc.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    char Table[NAME_MAX];
    PageTable Pages;
} Segment;

static t_dictionary* SegmentTable = NULL;
static char TablePath[PATH_MAX] = { 0 };

static void _segmentDestroy(void* segment);

typedef struct
{
    size_t* minCounter;
    size_t* minFrame;
    Segment** minPageSegment;
} SLRUPageParameters;
static void _saveLRUPage(char const* qualifiedPath, void* segment, void* param);

static inline void _printFullKey(char const* tableName, char* tablePathBuf)
{
    snprintf(tablePathBuf, PATH_MAX, "%sTables/%s", TablePath, tableName);
}

void SegmentTable_Initialize(char const* tablePath)
{
    SegmentTable = dictionary_create();

    snprintf(TablePath, PATH_MAX, "%s", tablePath);
}

PageTable* SegmentTable_CreateSegment(char const* tableName)
{
    Segment* s = Malloc(sizeof(Segment));
    snprintf(s->Table, NAME_MAX, "%s", tableName);
    PageTable_Construct(&s->Pages);

    char qualifiedPath[PATH_MAX];
    _printFullKey(tableName, qualifiedPath);
    dictionary_put(SegmentTable, qualifiedPath, s);

    return &s->Pages;
}

PageTable* SegmentTable_GetPageTable(char const* tableName)
{
    char qualifiedPath[PATH_MAX];
    _printFullKey(tableName, qualifiedPath);

    Segment* s = dictionary_get(SegmentTable, qualifiedPath);
    if (!s)
        return NULL;

    return &s->Pages;
}

bool SegmentTable_GetLRUFrame(size_t* frame)
{
    if (dictionary_is_empty(SegmentTable))
        return false;

    // entre los segmentos busco la pagina con menor timestamp
    size_t minCounter = 0;
    size_t minFrame = 0;
    Segment* minPageSegment = NULL;

    SLRUPageParameters p =
    {
        .minCounter = &minCounter,
        .minFrame = &minFrame,
        .minPageSegment = &minPageSegment
    };
    dictionary_iterator_with_data(SegmentTable, _saveLRUPage, &p);

    if (!minPageSegment)
        return false;

    *frame = minFrame;

    Frame* f = Memory_Read(minFrame);
    if (PageTable_PreemptPage(&minPageSegment->Pages, f->Key))
    {
        // era la ultima pagina de este segmento, borrarlo
        char qualifiedPath[PATH_MAX];
        _printFullKey(minPageSegment->Table, qualifiedPath);

        dictionary_remove_and_destroy(SegmentTable, qualifiedPath, _segmentDestroy);
    }

    return true;
}

static void _getDirtyFrames(char const* qualifiedPath, void* segment, void* vec)
{
    (void) qualifiedPath;

    Segment* const s = segment;
    PageTable_GetDirtyFrames(&s->Pages, s->Table, vec);
}

void SegmentTable_GetDirtyFrames(Vector* dirtyFrames)
{
    dictionary_iterator_with_data(SegmentTable, _getDirtyFrames, dirtyFrames);
}

void SegmentTable_DeleteSegment(char const* tableName)
{
    char qualifiedPath[PATH_MAX];
    _printFullKey(tableName, qualifiedPath);

    dictionary_remove_and_destroy(SegmentTable, qualifiedPath, _segmentDestroy);
}

void SegmentTable_Clean(void)
{
    dictionary_clean_and_destroy_elements(SegmentTable, _segmentDestroy);
}

void SegmentTable_Destroy(void)
{
    dictionary_destroy_and_destroy_elements(SegmentTable, _segmentDestroy);
}

/* PRIVATE */
static void _segmentDestroy(void* segment)
{
    Segment* const s = segment;
    PageTable_Destruct(&s->Pages);
    Free(s);
}

static void _saveLRUPage(char const* qualifiedPath, void* segment, void* param)
{
    (void) qualifiedPath;

    Segment* const s = segment;

    size_t lruFrame;
    size_t timestamp;
    bool hasLRU = PageTable_GetLRUFrame(&s->Pages, &lruFrame, &timestamp);

    SLRUPageParameters* const p = param;
    if (hasLRU && (!*p->minPageSegment || timestamp < *p->minCounter))
    {
        *p->minCounter = timestamp;
        *p->minFrame = lruFrame;
        *p->minPageSegment = s;
    }
}
