
#include "vector.h"
#include "Malloc.h"
#include <assert.h>
#include <string.h>

void vector_of_string_free_fn(void* pstr)
{
    char** string = pstr;
    Free(*string);
}

inline static void* _calculateOffset(void* array, size_t pos, size_t elemSize)
{
    return ((char*) array) + pos * elemSize;
}

static size_t _calculateGrowth(Vector const* v, size_t newSize)
{
    size_t geometric = v->Capacity + v->Capacity / 2;
    if (geometric < newSize)
        return newSize;
    return geometric;
}

void Vector_Construct(Vector* v, size_t elementSize, VectorFreeFn freeFn, size_t initialCapacity)
{
    v->Elements = NULL;
    v->ElemSize = elementSize;
    v->Size = 0;
    v->Capacity = initialCapacity;
    if (initialCapacity)
        v->Elements = Malloc(initialCapacity * elementSize);
    v->FreeFn = freeFn;
}

void Vector_Destruct(Vector* v)
{
    if (v->FreeFn)
        Vector_iterate(v, v->FreeFn);
    Free(v->Elements);
}

void Vector_iterate(Vector const* v, VectorClosureFn closureFn)
{
    for (size_t i = 0; i < v->Size; ++i)
        closureFn(_calculateOffset(v->Elements, i, v->ElemSize));
}

void Vector_iterate_with_data(Vector const* v, VectorClosureExtraFn closureFn, void* extraData)
{
    for (size_t i = 0; i < v->Size; ++i)
        closureFn(_calculateOffset(v->Elements, i, v->ElemSize), extraData);
}

size_t Vector_size(Vector const* v)
{
    return v->Size;
}

void Vector_resize(Vector* v, size_t n, void const* elem)
{
    size_t oldsize = v->Size;

    if (n < oldsize)
        Vector_erase_range(v, n, v->Size);
    else if (n > oldsize)
    {
        Vector_reserve(v, n);

        // fill rest with elem
        Vector_insert_fill(v, oldsize, n - oldsize, elem);
    }
}

void Vector_resize_zero(Vector* v, size_t n)
{
    size_t oldsize = v->Size;
    if (n < oldsize)
        Vector_erase_range(v, n, v->Size);
    else
    {
        Vector_reserve(v, n);

        memset(_calculateOffset(v->Elements, oldsize, v->ElemSize), 0, (n - oldsize) * v->ElemSize);
        v->Size = n;
    }
}

size_t Vector_capacity(Vector const* v)
{
    return v->Capacity;
}

bool Vector_empty(Vector const* v)
{
    return !v->Size;
}

void Vector_reserve(Vector* v, size_t n)
{
    if (n <= v->Capacity)
        return;

    v->Elements = Realloc(v->Elements, n * v->ElemSize);
    v->Capacity = n;
}

void Vector_shrink_to_fit(Vector* v)
{
    if (v->Capacity == v->Size)
        return;

    if (!v->Size)
    {
        Free(v->Elements);
        v->Elements = NULL;
        v->Capacity = 0;
    }
    else
    {
        v->Elements = Realloc(v->Elements, v->Size * v->ElemSize);
        v->Capacity = v->Size;
    }
}

void* Vector_at(Vector const* v, size_t i)
{
    assert(v->Elements && i < v->Size);
    return _calculateOffset(v->Elements, i, v->ElemSize);
}

void* Vector_front(Vector const* v)
{
    assert(v->Elements);
    return _calculateOffset(v->Elements, 0, v->ElemSize);
}

void* Vector_back(Vector const* v)
{
    assert(v->Elements && v->Size);
    return Vector_at(v, v->Size - 1);
}

void* Vector_data(Vector const* v)
{
    return v->Elements;
}

void Vector_push_back(Vector* v, void const* elem)
{
    if (v->Capacity == v->Size)
        Vector_reserve(v, _calculateGrowth(v, v->Size + 1));

    memcpy(_calculateOffset(v->Elements, v->Size, v->ElemSize), elem, v->ElemSize);
    ++v->Size;
}

void Vector_pop_back(Vector* v)
{
    assert(v->Size);
    Vector_erase(v, v->Size - 1);
}

void Vector_insert(Vector* v, size_t pos, void const* elem)
{
    Vector_insert_fill(v, pos, 1, elem);
}

void Vector_insert_fill(Vector* v, size_t pos, size_t n, void const* elem)
{
    assert(pos <= v->Size);
    if (n == 1 && pos == v->Size) // special case
    {
        Vector_push_back(v, elem);
        return;
    }

    if (n > (v->Capacity - v->Size))
        Vector_reserve(v, _calculateGrowth(v, v->Size + n));

    void* start = _calculateOffset(v->Elements, pos, v->ElemSize);
    void* end = _calculateOffset(v->Elements, pos + n, v->ElemSize);
    size_t const blockSize = (v->Size - pos) * v->ElemSize;

    memmove(end, start, blockSize);
    for (size_t i = 0; i < n; ++i)
        memcpy(_calculateOffset(start, i, v->ElemSize), elem, v->ElemSize);
    v->Size += n;
}

void Vector_insert_range(Vector* v, size_t pos, void* begin, void* end)
{
    assert(pos <= v->Size);
    if (begin == end)
        return;

    size_t n = ((char*)end - (char*)begin) / v->ElemSize;
    if (n == 1 && pos == v->Size) // special case
    {
        Vector_push_back(v, begin);
        return;
    }

    if (n > (v->Capacity - v->Size))
        Vector_reserve(v, _calculateGrowth(v, v->Size + n));

    void* shiftSrc = _calculateOffset(v->Elements, pos, v->ElemSize);
    void* shiftDst = _calculateOffset(v->Elements, pos + n, v->ElemSize);
    size_t const blockSize = (v->Size - pos) * v->ElemSize;

    if (blockSize)
        memmove(shiftDst, shiftSrc, blockSize);

    memcpy(shiftSrc, begin, n * v->ElemSize);
    v->Size += n;
}

void Vector_erase(Vector* v, size_t pos)
{
    assert(pos < v->Size);
    Vector_erase_range(v, pos, pos + 1);
}

void Vector_erase_range(Vector* v, size_t begin, size_t end)
{
    assert(begin < v->Size);
    assert(end <= v->Size);

    size_t n = end - begin;
    if (v->FreeFn)
    {
        for (size_t i = begin; i < end; ++i)
            v->FreeFn(_calculateOffset(v->Elements, i, v->ElemSize));
    }

    // shift needed
    if (end < v->Size)
    {
        void* shiftSrc = _calculateOffset(v->Elements, end, v->ElemSize);
        void* shiftDst = _calculateOffset(v->Elements, begin, v->ElemSize);
        size_t const blockSize = (v->Size - end) * v->ElemSize;
        memmove(shiftDst, shiftSrc, blockSize);
    }

    v->Size -= n;
}

void Vector_swap(Vector* v, Vector* other)
{
    //pod should copy
    Vector me = *v;
    *v = *other;
    *other = me;
}

void Vector_adopt(Vector* v, void* buf, size_t bufSize)
{
    Vector other =
    {
        .Elements = buf,
        .ElemSize = 1,
        .Size = bufSize,
        .Capacity = bufSize,
        .FreeFn = NULL
    };
    Vector_swap(v, &other);
}

void Vector_clear(Vector* v)
{
    if (v->FreeFn)
        Vector_iterate(v, v->FreeFn);
    v->Size = 0;
}
