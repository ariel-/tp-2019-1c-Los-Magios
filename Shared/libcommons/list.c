/*
 * Copyright (C) 2012 Sistemas Operativos - UTN FRBA. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "list.h"
#include "Malloc.h"

static void list_link_element(t_link_element* previous, t_link_element* next);
static t_link_element* list_create_element(void* data);
static t_link_element* list_get_element(t_link_element* head, size_t index);
static t_link_element* list_find_element(t_list* self, bool(*condition)(void*, void*), void* extra, size_t* index);
static void _add_in_list(void* element, void* self);
static void list_sort_aux(t_list* self, t_link_element* bfirst, t_link_element* last, bool(*)(void*, void*), size_t size);

t_list* list_create(void)
{
    t_list* list = Malloc(sizeof(t_list));
    list->head = NULL;
    list->tail = NULL;
    list->elements_count = 0;
    return list;
}

void list_add(t_list* self, void* data)
{
    t_link_element* new_element = list_create_element(data);

    if (self->elements_count == 0)
    {
        self->head = new_element;
        self->tail = new_element;
    }
    else
    {
        list_link_element(self->tail, new_element);
        self->tail = new_element;
    }

    ++self->elements_count;
}

void list_prepend(t_list* self, void* data)
{
    t_link_element* new_element = list_create_element(data);

    if (self->elements_count == 0)
    {
        self->head = new_element;
        self->tail = new_element;
    }
    else
    {
        list_link_element(new_element, self->head);
        self->head = new_element;
    }

    ++self->elements_count;
}

void list_add_all(t_list* self, t_list* other)
{
    list_iterate_with_data(other, _add_in_list, self);
}

void* list_get(t_list* self, size_t index)
{
    t_link_element* element_in_index = list_get_element(self->head, index);
    return element_in_index != NULL ? element_in_index->data : NULL;
}

void list_add_in_index(t_list* self, size_t index, void* data)
{
    // caso especial index == elements_count (agregar al final)
    if (index > self->elements_count)
        return;

    if (!index)
    {
        // inicio
        list_prepend(self, data);
    }
    else if (index == self->elements_count)
    {
        // fin
        list_add(self, data);
    }
    else
    {
        // cualquier otro lugar
        t_link_element* new_element = list_create_element(data);
        t_link_element* prev = list_get_element(self->head, index - 1);
        list_link_element(new_element, prev->next);
        list_link_element(prev, new_element);

        ++self->elements_count;
    }
}

void* list_replace(t_list* self, size_t index, void* data)
{
    void* old_data = NULL;
    t_link_element* element = list_get_element(self->head, index);
    if (element != NULL)
    {
        old_data = element->data;
        element->data = data;
    }

    return old_data;
}

void list_replace_and_destroy_element(t_list* self, size_t index, void* data, void(* element_destroyer)(void*))
{
    void* old_data = list_replace(self, index, data);
    element_destroyer(old_data);
}

void* list_find(t_list* self, bool(*condition)(void*, void*), void* extra)
{
    t_link_element* element = list_find_element(self, condition, extra, NULL);
    return element != NULL ? element->data : NULL;
}

void list_iterate(t_list* self, void(*closure)(void*))
{
    for (t_link_element* p = self->head; p != NULL;)
    {
        t_link_element* next = p->next;
        closure(p->data);
        p = next;
    }
}

void list_iterate_with_data(t_list* self, void(*closure)(void*, void*), void* extra)
{
    for (t_link_element* p = self->head; p != NULL;)
    {
        t_link_element* next = p->next;
        closure(p->data, extra);
        p = next;
    }
}

void* list_remove(t_list* self, size_t index)
{
    if (index >= self->elements_count)
        return NULL;

    t_link_element* aux_element = list_get_element(self->head, index);
    void* data = aux_element->data;

    if (index == 0)
        self->head = aux_element->next;
    else
    {
        t_link_element* previous = list_get_element(self->head, index - 1);
        list_link_element(previous, aux_element->next);

        if (aux_element == self->tail)
            self->tail = previous;
    }

    --self->elements_count;
    Free(aux_element);
    return data;
}

void* list_remove_by_condition(t_list* self, bool(*condition)(void*, void*), void* extra)
{
    size_t index = 0;
    t_link_element* element = list_find_element(self, condition, extra, &index);
    if (element != NULL)
        return list_remove(self, index);

    return NULL;
}

void list_remove_and_destroy_element(t_list* self, size_t index, void(*element_destroyer)(void*))
{
    void* data = list_remove(self, index);
    element_destroyer(data);
}

void list_remove_and_destroy_by_condition(t_list* self, bool(* condition)(void*, void*), void* extra, void(* element_destroyer)(void*))
{
    void* data = list_remove_by_condition(self, condition, extra);
    if (data)
        element_destroyer(data);
}

size_t list_size(t_list const* list)
{
    return list->elements_count;
}

bool list_is_empty(t_list const* list)
{
    return !list->elements_count;
}

void list_clean(t_list* self)
{
    for (t_link_element* p = self->head; p != NULL;)
    {
        t_link_element* next = p->next;
        Free(p);
        p = next;
    }

    self->elements_count = 0;
    self->head = NULL;
    self->tail = NULL;
}

void list_clean_and_destroy_elements(t_list* self, void(*element_destroyer)(void*))
{
    list_iterate(self, element_destroyer);
    list_clean(self);
}

void list_destroy(t_list* self)
{
    list_clean(self);
    Free(self);
}

void list_destroy_and_destroy_elements(t_list* self, void(*element_destroyer)(void*))
{
    list_clean_and_destroy_elements(self, element_destroyer);
    Free(self);
}

t_list* list_take(t_list* self, size_t count)
{
    t_list* sublist = list_create();

    if (count > self->elements_count)
        count = self->elements_count;

    t_link_element* p = self->head;
    for (size_t i = 0; i < count; ++i, p = p->next)
        list_add(sublist, p->data);

    return sublist;
}

t_list* list_take_and_remove(t_list* self, size_t count)
{
    t_list* sublist = list_create();

    if (count > self->elements_count)
        count = self->elements_count;

    t_link_element* p = self->head;
    for (size_t i = 0; i < count; ++i, p = p->next)
        list_add(sublist, p->data);

    if (!p)
        list_clean(self);
    else
    {
        self->elements_count -= count;
        self->head = p;
    }

    return sublist;
}

t_list* list_filter(t_list* self, bool(*condition)(void*))
{
    t_list* filtered = list_create();
    for (t_link_element* p = self->head; p != NULL; p = p->next)
        if (condition(p->data))
            list_add(filtered, p->data);

    return filtered;
}

t_list* list_map(t_list* self, void* (*transformer)(void*))
{
    t_list* mapped = list_create();
    for (t_link_element* p = self->head; p != NULL; p = p->next)
        list_add(mapped, transformer(p->data));

    return mapped;
}

void list_sort(t_list* self, bool (*comparator)(void*, void*))
{
    // nodo inventado pre-head
    t_link_element bHead;
    bHead.data = NULL; // no me interesa el data pero lo inicializo igual
    bHead.next = self->head;

    list_sort_aux(self, &bHead, NULL, comparator, self->elements_count);
}

t_list* list_sorted(t_list* self, bool (*comparator)(void*, void*))
{
    t_list* duplicated = list_duplicate(self);
    list_sort(duplicated, comparator);
    return duplicated;
}

size_t list_count_satisfying(t_list* self, bool(*condition)(void*))
{
    t_list* satisfying = list_filter(self, condition);
    size_t result = satisfying->elements_count;
    list_destroy(satisfying);
    return result;
}

bool list_any_satisfy(t_list* self, bool(*condition)(void*))
{
    return list_count_satisfying(self, condition) > 0;
}

bool list_all_satisfy(t_list* self, bool(*condition)(void*))
{
    return list_count_satisfying(self, condition) == self->elements_count;
}

t_list* list_duplicate(t_list* self)
{
    t_list* duplicated = list_create();
    list_add_all(duplicated, self);
    return duplicated;
}

void* list_reduce(t_list* self, void* seed, void*(*operation)(void*, void*))
{
    void* result = seed;
    for (t_link_element* p = self->head; p != NULL; p = p->next)
        result = operation(result, p->data);

    return result;
}

/********* PRIVATE FUNCTIONS **************/

static void list_link_element(t_link_element* previous, t_link_element* next)
{
    if (previous != NULL)
        previous->next = next;
}

static t_link_element* list_create_element(void* data)
{
    t_link_element* element = Malloc(sizeof(t_link_element));
    element->data = data;
    element->next = NULL;
    return element;
}

static t_link_element* list_get_element(t_link_element* head, size_t index)
{
    t_link_element* p = head;
    for (; p != NULL && index > 0; p = p->next, --index);

    // mayor a longitud
    if (index > 0)
        return NULL;

    return p;
}

static t_link_element* list_find_element(t_list* self, bool(*condition)(void*, void*), void* extra, size_t* index)
{
    t_link_element* element = self->head;
    size_t position = 0;

    while (element != NULL && !condition(element->data, extra))
    {
        element = element->next;
        ++position;
    }

    if (index)
        *index = position;

    return element;
}

static void _add_in_list(void* element, void* self)
{
    list_add(self, element);
}

static void list_sort_aux(t_list* self, t_link_element* before_first, t_link_element* last, bool(*comparator)(void*, void*), size_t size)
{
    if (size < 2)
        return;

    size_t const halfSize = size / 2;
    t_link_element* mid = list_get_element(before_first, 1 + halfSize);
    list_sort_aux(self, before_first, mid, comparator, halfSize);
    t_link_element* first = before_first->next;

    t_link_element* before_mid = list_get_element(before_first, halfSize);
    list_sort_aux(self, before_mid, last, comparator, size - halfSize);
    mid = before_mid->next;

    for (;;)
    {
        // [first, mid) y [mid, last) estan ordenados y son no vacios
        // mid < first? entonces mid primero, tengo que intercambiar nodos
        if (comparator(mid->data, first->data))
        {
            t_link_element* after = before_mid->next;
            if (!after) // before_mid == self->tail; ignore
                continue;

            if (after == self->tail)
                self->tail = before_mid;

            list_link_element(before_mid, after->next);

            if (before_first->next == self->head)
                self->head = after;

            t_link_element* next = before_first->next;
            list_link_element(before_first, after);
            list_link_element(after, next);

            before_first = before_first->next;
            mid = before_mid->next;
            if (mid == last)
                return; // termino [_Mid, _Last); listo
        }
        else
        {
            // mid >= first entonces first primero, dejo los nodos como están
            before_first = before_first->next;
            first = first->next;
            if (first == mid)
                return; // termino [_First, _Mid); listo
        }
    }
}
