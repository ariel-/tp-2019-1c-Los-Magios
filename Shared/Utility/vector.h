
#ifndef vector_h__
#define vector_h__

#include <stdbool.h>
#include <stddef.h>

/*
 * Función de liberación de elemento (free o similar)
 */
typedef void (*VectorFreeFn)(void* element);
typedef void (*VectorClosureFn)(void* element);
typedef void (*VectorClosureExtraFn)(void* element, void* extraData);

void vector_of_string_free_fn(void* pstr);

typedef struct
{
    void* Elements;
    size_t ElemSize;
    size_t Size;
    size_t Capacity;
    VectorFreeFn FreeFn;
} Vector;

#define VECTOR_OF_STRINGS_INITIALIZER \
    { NULL, sizeof(char*), 0, 0, vector_of_string_free_fn }

void Vector_Construct(Vector* v, size_t elementSize, VectorFreeFn freeFn, size_t initialCapacity);

void Vector_Destruct(Vector* v);

// Non-C++ interface
/*
 * Itera los elementos con una función que no requiere parámetros adicionales
 */
void Vector_iterate(Vector const* v, VectorClosureFn closureFn);

/*
 * Itera los elementos con una función.
 * Dado que los closures son una extensión GNU y no compilan con el parámetro -pedantic
 * Se permite pasar una estructura definida por el usuario a la función.
 */
void Vector_iterate_with_data(Vector const* v, VectorClosureExtraFn closureFn, void* extraData);


// C++ like interface
/*
 * Devuelve tamaño del vector
 */
size_t Vector_size(Vector const* v);

/*
 * Cambia el tamaño del vector eliminando o insertando elementos
 * Los elementos insertados serán copias del elemento pasado por parámetro
 */
void Vector_resize(Vector* v, size_t n, void const* elem);

/*
 * Cambia el tamaño del vector eliminando o insertando elementos
 * Los elementos insertados serán inicializados en 0
 */
void Vector_resize_zero(Vector* v, size_t n);

/*
 * Devuelve tamaño de la capacidad en elementos actual
 */
size_t Vector_capacity(Vector const* v);

/*
 * Devuelve true ssi el vector está vacio
 */
bool Vector_empty(Vector const* v);

/*
 * Se asegura que la capacidad alocada sea de al menos n elementos
 * No cambia ningún elemento actual
 */
void Vector_reserve(Vector* v, size_t n);

/*
 * Encoge la capacidad del vector de forma tal que sólo quepan
 * sus elementos actuales. A diferencia del estándar, siempre efectúa la operación
 * en caso de ser necesaria.
 */
void Vector_shrink_to_fit(Vector* v);

/*
 * Devuelve el elemento en la posición i-ésima.
 * Observar que el puntero devuelto puede no ser válido
 * luego de operaciones que modifiquen el estado
 */
void* Vector_at(Vector const* v, size_t i);

/*
 * Devuelve el primer elemento
 */
void* Vector_front(Vector const* v);

/*
 * Devuelve el último elemento
 */
void* Vector_back(Vector const* v);

/*
 * Devuelve el arreglo de datos
 */
void* Vector_data(Vector const* v);

/*
 * Inserta un elemento al final
 */
void Vector_push_back(Vector* v, void const* elem);

/*
 * Elimina el último elemento
 */
void Vector_pop_back(Vector* v);

/*
 * Inserta un único valor en la posición pos
 */
void Vector_insert(Vector* v, size_t pos, void const* elem);

/*
 * Inserta n copias del elemento elem en la posición pos
 */
void Vector_insert_fill(Vector* v, size_t pos, size_t n, void const* elem);

/*
 * Inserta valores desde un arreglo, entre la posicion begin, hasta la end exclusive
 * En otras palabras, rango cerrado abierto [begin, end)
 */
void Vector_insert_range(Vector* v, size_t pos, void* begin, void* end);

/*
 * Borra el elemento ubicado en pos
 */
void Vector_erase(Vector* v, size_t pos);

/*
 * Borra los elementos en el rango de posiciones cerrado abierto [begin, end)
 */
void Vector_erase_range(Vector* v, size_t begin, size_t end);

/*
 * Intercambia contenidos con otro vector
 */
void Vector_swap(Vector* v, Vector* other);

/*
 * Toma posesion de un bufer en memoria, el vector pasa a administrar la memoria
 */
void Vector_adopt(Vector* v, void* buf, size_t bufSize);

/*
 * Limpia los elementos
 */
void Vector_clear(Vector* v);

#endif //vector_h__
