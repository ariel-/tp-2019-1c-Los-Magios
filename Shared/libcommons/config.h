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

/*
 * Modificaciones por ariel-: 20190423
 *  - Borro los newlines windows style en lugar de hacer un escándalo por ello xD
 *  - Modificaciones para que compile en ANSI C
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include "dictionary.h"
#include <stdbool.h>
#include <vector.h>

typedef struct
{
    char* path;
    t_dictionary* properties;
} t_config;

/**
* @NAME: config_create
* @DESC: Crea y devuelve un puntero a una estructura t_config
* @PARAMS:
* 		path - path del archivo de configuracion
*/
t_config* config_create(char const* path);

/**
* @NAME: config_has_property
* @DESC: Retorna true si key se encuentra en la configuracion.
*/
bool config_has_property(t_config const*, char const* key);

/**
* @NAME: config_get_string_value
* @DESC: Retorna un string con el valor asociado a key.
*/
char* config_get_string_value(t_config const*, char const* key);

/**
* @NAME: config_get_int_value
* @DESC:Retorna un int con el valor asociado a key.
*/
int config_get_int_value(t_config const*, char const* key);

/**
* @NAME: config_get_long_value
* @DESC:Retorna un long con el valor asociado a key.
*/
long config_get_long_value(t_config const*, char const* key);

/**
* @NAME: config_get_double_value
* @DESC:Retorna un double con el valor asociado a key.
*/
double config_get_double_value(t_config const*, char const* key);

/**
* @NAME: config_get_array_value
* @DESC: Retorna un array con los valores asociados a la key especificada.
* En el archivo de configuracion un valor de este tipo debería ser representado
* de la siguiente forma [lista_valores_separados_por_coma]
* Ejemplo:
* VALORES=[1,2,3,4,5]
* El array que devuelve termina en NULL
*/
Vector config_get_array_value(t_config const*, char const* key);

/**
* @NAME: config_key_amount
* @DESC: Retorna la cantidad de keys.
*/
int config_keys_amount(t_config const*);

/**
* @NAME: config_destroy
* @DESC: Destruye la estructura config.
*/
void config_destroy(t_config* config);

/**
* @NAME: config_set_value
* @DESC: Setea el valor en el archivo de config, a la key asociada.
*/
void config_set_value(t_config*, char const* key, char const* value);

/**
* @NAME: config_set_array_value
* @DESC: Setea el valor array en el archivo de config, a la key asociada.
*/
void config_set_array_value(t_config*, char const* key, Vector const* vec);

/**
* @NAME: config_remove_key
* @DESC: Remueve la clave y su valor asociado del archivo de config.
*/
void config_remove_key(t_config*, char const* key);

/**
* @NAME: config_save
* @DESC: Reescribe el archivo de configuracion con los valores del config.
*/
int config_save(t_config*);

/**
* @NAME: config_save_in_file
* @DESC: Escribe un archivo de configuracion en el path indicado con los valores del config.
*/
int config_save_in_file(t_config*, char const* path);

#endif /* CONFIG_H_ */
