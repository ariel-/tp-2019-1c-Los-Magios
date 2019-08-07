
#include "PageTable.h"
#include "MainMemory.h"
#include <Malloc.h>

// contador de uso, común a todas las tablas de página
static size_t MonotonicCounter = 0;

typedef struct
{
    size_t Counter;
    size_t Frame;
    bool Dirty;
} Page;

typedef struct
{
    size_t* minCounter;
    Page** minPage;
}  SLRUPageParameters;
static void _saveLRUPage(int key, void* page, void* param);

typedef struct
{
    char const* tableName;
    Vector* dirtyFrames;
} GDFrameParameters;
static void _addDirtyFrame(int key, void* page, void* param);

static void _cleanPage(void* page);

void PageTable_Construct(PageTable* pt)
{
    pt->Pages = hashmap_create();
}

void PageTable_AddPage(PageTable* pt, uint16_t key, size_t frame)
{
    Page* p = Malloc(sizeof(Page));
    p->Counter = ++MonotonicCounter;
    p->Frame = frame;
    p->Dirty = false;

    hashmap_put(pt->Pages, key, p);
}

bool PageTable_GetLRUFrame(PageTable const* pt, size_t* frame, size_t* timestamp)
{
    size_t minCounter = 0;
    Page* minPage = NULL;

    SLRUPageParameters p =
    {
        .minCounter = &minCounter,
        .minPage = &minPage
    };
    hashmap_iterate_with_data(pt->Pages, _saveLRUPage, &p);

    if (!minPage)
        return false;

    *timestamp = minPage->Counter;
    *frame = minPage->Frame;

    return true;
}

void PageTable_GetDirtyFrames(PageTable const* pt, char const* tableName, Vector* dirtyFrames)
{
    GDFrameParameters p =
    {
        .tableName = tableName,
        .dirtyFrames = dirtyFrames
    };

    hashmap_iterate_with_data(pt->Pages, _addDirtyFrame, &p);
}

bool PageTable_PreemptPage(PageTable* pt, uint16_t key)
{
    hashmap_remove_and_destroy(pt->Pages, key, _cleanPage);
    return hashmap_is_empty(pt->Pages);
}

bool PageTable_GetFrameNumber(PageTable const* pt, uint16_t key, size_t* page)
{
    Page* p = hashmap_get(pt->Pages, key);
    if (!p)
        return false;

    // increment last used
    p->Counter = ++MonotonicCounter;
    *page = p->Frame;
    return true;
}

void PageTable_MarkDirty(PageTable const* pt, uint16_t key)
{
    Page* p = hashmap_get(pt->Pages, key);
    if (!p)
        return;

    p->Dirty = true;
}

void PageTable_Destruct(PageTable* pt)
{
    hashmap_destroy_and_destroy_elements(pt->Pages, _cleanPage);
}

/* PRIVATE */
static void _saveLRUPage(int key, void* page, void* param)
{
    (void) key;

    Page* const p = page;

    SLRUPageParameters* const data = param;
    if (!p->Dirty && (!*data->minPage || p->Counter < *data->minCounter))
    {
        *data->minCounter = p->Counter;
        *data->minPage = p;
    }
}

static void _addDirtyFrame(int key, void* page, void* param)
{
    (void) key;

    Page* const p = page;
    if (!p->Dirty)
        return;

    Frame* f = Memory_Read(p->Frame);
    GDFrameParameters* const data = param;
    DirtyFrame df =
    {
        .TableName = data->tableName,
        .Frame = f
    };

    Vector_push_back(data->dirtyFrames, &df);
}

static void _cleanPage(void* page)
{
    Page* const p = page;
    Memory_CleanFrame(p->Frame);
    Free(p);
}
