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

#include "dictionary.h"
#include "config.h"
#include "string.h"
#include <File.h>
#include <Malloc.h>
#include <stdlib.h>

static void add_configuration(char* line, void* cfg)
{
    t_config* const config = cfg;
    if (!string_starts_with(line, "#"))
    {
        Vector keyAndValue = string_n_split(line, 2, "=");
        if (Vector_size(&keyAndValue) != 2)
        {
            // config con error
            Vector_Destruct(&keyAndValue);
            return;
        }

        char** const tokens = Vector_data(&keyAndValue);

        char* const key = string_duplicate(tokens[0]);
        char* const value = string_duplicate(tokens[1]);
        dictionary_put(config->properties, key, value);
        Free(key); // el diccionario copia la key
        Vector_Destruct(&keyAndValue);
    }
}

t_config* config_create(char const* path)
{
    t_config* config = NULL;

    File* file = file_open(path, F_OPEN_READ);
    if (file_is_open(file))
    {
        config = Malloc(sizeof(t_config));
        config->path = string_duplicate(path);
        config->properties = dictionary_create();

        Vector lines = file_getlines(file);
        string_iterate_lines_with_data(&lines, add_configuration, config);
        Vector_Destruct(&lines);
    }

    file_close(file);
    return config;
}

bool config_has_property(t_config const* self, char const* key)
{
    return dictionary_has_key(self->properties, key);
}

char* config_get_string_value(t_config const* self, char const* key)
{
    return dictionary_get(self->properties, key);
}

int config_get_int_value(t_config const* self, char const* key)
{
    char* value = config_get_string_value(self, key);
    return strtol(value, NULL, 10);
}

long config_get_long_value(t_config const* self, char const* key)
{
    char* value = config_get_string_value(self, key);
    return strtol(value, NULL, 10);
}

double config_get_double_value(t_config const* self, char const* key)
{
    char* value = config_get_string_value(self, key);
    return strtod(value, NULL);
}

Vector config_get_array_value(t_config const* self, char const* key)
{
    char* value_in_dictionary = config_get_string_value(self, key);
    return string_get_string_as_array(value_in_dictionary);
}

int config_keys_amount(t_config const* self)
{
    return dictionary_size(self->properties);
}

void config_destroy(t_config* config)
{
    dictionary_destroy_and_destroy_elements(config->properties, Free);
    Free(config->path);
    Free(config);
}

void config_set_value(t_config* self, char const* key, char const* value)
{
    config_remove_key(self, key);
    char* duplicate_value = string_duplicate(value);
    dictionary_put(self->properties, key, duplicate_value);
}

void config_set_array_value(t_config* self, char const* key, Vector const* vec)
{
    if (Vector_empty(vec))
    {
        config_set_value(self, key, "[]");
        return;
    }

    char* value = string_from_format("%s", "[");

    size_t const begin = 0;
    size_t const end = Vector_size(vec) - 1;

    size_t i = 0;
    char** const strings = Vector_data(vec);
    for (i = begin; i < end; ++i)
        string_append_with_format(&value, "%s,", strings[i]);
    //last element
    string_append_with_format(&value, "%s]", strings[i]);

    config_remove_key(self, key);
    dictionary_put(self->properties, key, value);
}

void config_remove_key(t_config* self, char const* key)
{
    t_dictionary* dictionary = self->properties;
    if (dictionary_has_key(dictionary, key))
        dictionary_remove_and_destroy(dictionary, key, Free);
}

int config_save(t_config* self)
{
    return config_save_in_file(self, self->path);
}

static void _addLine(char const* key, void* val, void* vec)
{
    char* line = string_from_format("%s=%s", key, val);
    Vector_push_back(vec, &line);
}

int config_save_in_file(t_config* self, char const* path)
{
    File* file = file_open(path, F_OPEN_WRITE);
    if (!file_is_open(file))
    {
        file_close(file);
        return -1;
    }

    Vector lines = VECTOR_OF_STRINGS_INITIALIZER;
    dictionary_iterator_with_data(self->properties, _addLine, &lines);

    int result = file_writelines(file, &lines);
    file_close(file);
    Vector_Destruct(&lines);
    return result;
}
