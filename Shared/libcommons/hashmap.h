/*
 * Copyright (C) 2012 Sistemas Operativos - UTN FRBA. All rights reserved.
 *
 * This program is Free software: you can redistribute it and/or modify
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

/**
 * Modificaciones by ariel-: 20190423
 * Cambio de nombre a hashmap
 * Cambio de key por 'int', este diccionario se va a usar para mapear fds con su abstracción
 * Ahora utiliza el TAD "vector" en lugar de hacer los alloc a mano
 */

#ifndef HashMap_h__
#define HashMap_h__

#define DEFAULT_HASHMAP_INITIAL_SIZE 20

#include "vector.h"
#include <stdbool.h>

typedef struct
{
    Vector elements;
    unsigned int table_max_size;
    unsigned int table_current_size;
    unsigned int elements_amount;
} t_hashmap;

/**
* @NAME: hashmap_create
* @DESC: Crea el diccionario
*/
t_hashmap* hashmap_create(void);

/**
* @NAME: hashmap_put
* @DESC: Inserta un nuevo par (key->value) al diccionario, en caso de ya existir la key actualiza el value.
* [Warning] - Tener en cuenta que esto no va a liberar la memoria del `value` original.
*/
void hashmap_put(t_hashmap* d, int key, void* val);

/**
* @NAME: hashmap_get
* @DESC: Obtiene value asociado a key.
*/
void* hashmap_get(t_hashmap const* d, int key);

/**
* @NAME: hashmap_remove
* @DESC: Remueve un elemento del diccionario y lo retorna.
*/
void* hashmap_remove(t_hashmap* d, int key);

/**
* @NAME: hashmap_remove_and_destroy
* @DESC: Remueve un elemento del diccionario y lo destruye.
*/
void hashmap_remove_and_destroy(t_hashmap* d, int key, void(*data_destroyer)(void* val));

/**
* @NAME: hashmap_iterate
* @DESC: Aplica closure a todos los elementos del diccionario.
*
* La función que se pasa por paremtro recibe (int key, void* value)
*/
void hashmap_iterate(t_hashmap const* d, void(*closure)(int key, void* val));

/**
* @NAME: hashmap_iterate_with_data
* @DESC: Aplica closure a todos los elementos del diccionario.
* Se permite pasar una estructura definida por el usuario a la función.
*
* La función que se pasa por paremtro recibe (int key, void* value, void* data)
*/
void hashmap_iterate_with_data(t_hashmap const* d, void(*closure)(int key, void* val, void* data), void* data);

/**
* @NAME: hashmap_clean
* @DESC: Quita todos los elementos del diccionario
*/
void hashmap_clean(t_hashmap* d);

/**
* @NAME: hashmap_clean_and_destroy_elements
* @DESC: Quita todos los elementos del diccionario y los destruye
*/
void hashmap_clean_and_destroy_elements(t_hashmap* d, void(*data_destroyer)(void* val));

/**
* @NAME: hashmap_has_key
* @DESC: Retorna true si key se encuentra en el diccionario
*/
bool hashmap_has_key(t_hashmap const* d, int key);

/**
* @NAME: hashmap_is_empty
* @DESC: Retorna true si el diccionario está vacío
*/
bool hashmap_is_empty(t_hashmap const* d);

/**
* @NAME: hashmap_size
* @DESC: Retorna la cantidad de elementos del diccionario
*/
unsigned int hashmap_size(t_hashmap const* d);

/**
* @NAME: hashmap_destroy
* @DESC: Destruye el diccionario
*/
void hashmap_destroy(t_hashmap* d);

/**
* @NAME: hashmap_destroy_and_destroy_elements
* @DESC: Destruye el diccionario y destruye sus elementos
*/
void hashmap_destroy_and_destroy_elements(t_hashmap* d, void(*data_destroyer)(void* val));

#endif /* HashMap_h__ */
