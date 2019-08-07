
#ifndef SegmentTable_h__
#define SegmentTable_h__

#include "PageTable.h"

void SegmentTable_Initialize(char const* tablePath);

PageTable* SegmentTable_CreateSegment(char const* tableName);

PageTable* SegmentTable_GetPageTable(char const* tableName);

bool SegmentTable_GetLRUFrame(size_t* frame);

void SegmentTable_GetDirtyFrames(Vector* dirtyFrames);

void SegmentTable_DeleteSegment(char const* tableName);

void SegmentTable_Clean(void);

void SegmentTable_Destroy(void);

#endif //SegmentTable_h__
