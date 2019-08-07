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

#include "string.h"
#include "Malloc.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static void _string_do(char* text, void (* closure)(char*));
static void _string_lower_element(char* ch);
static void _string_upper_element(char* ch);
static void _string_append_with_format_list(const char* format, char** original, va_list arguments);
static bool _is_last_token(char const* next, size_t index, size_t _);
static bool _is_last_n_token(char const* next, size_t index, size_t n);
static Vector _string_split(char const* text, char const* separator, bool(*condition)(char const*, size_t, size_t), size_t);

char* string_repeat(char character, int count)
{
    char* text = Calloc(count + 1, 1);
    int i = 0;
    for (i = 0; i < count; ++i)
    {
        text[i] = character;
    }
    return text;
}

char* string_duplicate(char const* original)
{
    return strdup(original);
}

void string_append(char** original, char const* string_to_add)
{
    *original = Realloc(*original, strlen(*original) + strlen(string_to_add) + 1);
    strcat(*original, string_to_add);
}

char* string_new()
{
    return string_duplicate("");
}

char* string_from_format(const char* format, ...)
{
    char* nuevo;
    va_list arguments;
    va_start(arguments, format);
    nuevo = string_from_vformat(format, arguments);
    va_end(arguments);
    return nuevo;
}

char* string_from_vformat(const char* format, va_list arguments)
{
    char* nuevo = string_new();
    _string_append_with_format_list(format, &nuevo, arguments);
    return nuevo;
}

char* string_itoa(int number)
{
    return string_from_format("%d", number);
}

void string_append_with_format(char** original, const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    _string_append_with_format_list(format, original, arguments);
    va_end(arguments);
}

void string_to_upper(char* text)
{
    _string_do(text, &_string_upper_element);
}

void string_to_lower(char* text)
{
    _string_do(text, &_string_lower_element);
}

void string_capitalized(char* text)
{
    if (!string_is_empty(text))
    {
        _string_upper_element(text);
        if (strlen(text) >= 2)
        {
            string_to_lower(&text[1]);
        }
    }
}

void string_trim(char** text)
{
    string_trim_left(text);
    string_trim_right(text);
}

void string_trim_left(char** text)
{
    char* string_without_blank = *text;

    while (isblank(*string_without_blank))
    {
        string_without_blank++;
    }

    char* new_string = string_duplicate(string_without_blank);

    Free(*text);
    *text = new_string;
}

void string_trim_right(char** text)
{
    char* string_without_blank = *text;
    int i = strlen(*text) - 1;
    while (i >= 0 && isspace(string_without_blank[i]))
    {
        string_without_blank[i] = '\0';
        i--;
    }
    *text = Realloc(*text, strlen(string_without_blank) + 1);
}

bool string_is_empty(char* text)
{
    return strlen(text) == 0;
}

bool string_starts_with(char const* text, char const* begin)
{
    return strncmp(text, begin, strlen(begin)) == 0;
}

bool string_ends_with(char const* text, char const* end)
{
    if (strlen(text) < strlen(end))
        return false;

    int index = strlen(text) - strlen(end);
    return strcmp(&text[index], end) == 0;
}

bool string_equals_ignore_case(char const* actual, char const* expected)
{
    return strcasecmp(actual, expected) == 0;
}

Vector string_split(char const* text, char const* separator)
{
    return _string_split(text, separator, _is_last_token, 0);
}

Vector string_q_split(char const* src, char sep)
{
    // hay que parsear manualmente
    Vector substrings = VECTOR_OF_STRINGS_INITIALIZER;

    bool insideQuotationMarks = false;
    char const* posold = src;
    char const* posnew = src;

    for ( ; ; ++posnew)
    {
        char const c = *posnew;
        bool const foundQuotationMark = (c == '\"');
        if (!insideQuotationMarks && foundQuotationMark)
        {
            insideQuotationMarks = true;
            ++posold; // skip el propio quote
            continue;
        }

        // agrego 1 substring si:
        // encuentro un separador suelto (no dentro de un quote)
        // encuentro el fin del quote
        // encuentro el fin del string
        if ((c == sep && !insideQuotationMarks) || (foundQuotationMark && insideQuotationMarks) || c == '\0')
        {
            if (foundQuotationMark)
                insideQuotationMarks = false;

            if (posold != posnew)
            {
                size_t const tokenLen = posnew - posold;
                char* elem = Malloc(tokenLen + 1);
                *elem = '\0';
                strncat(elem, posold, tokenLen);

                Vector_push_back(&substrings, &elem);
            }

            if (c == '\0')
                break;

            posold = posnew + 1;
        }
    }

    return substrings;
}

Vector string_n_split(char const* text, size_t n, char const* separator)
{
    return _string_split(text, separator, _is_last_n_token, n);
}

Vector string_get_string_as_array(char* text)
{
    size_t length_value = strlen(text) - 2;
    char* value_without_brackets = string_substring(text, 1, length_value);
    Vector array_values = string_split(value_without_brackets, ",");

    for (size_t i = 0; i < Vector_size(&array_values); ++i)
        string_trim(Vector_at(&array_values, i));

    Free(value_without_brackets);
    return array_values;
}

char* string_substring(char const* text, size_t start, size_t length)
{
    char* new_string = Calloc(1, length + 1);
    strncpy(new_string, text + start, length);
    return new_string;
}

char* string_substring_from(char const* text, int start)
{
    return string_substring(text, start, strlen(text) - start);
}

char* string_substring_until(char const* text, int length)
{
    return string_substring(text, 0, length);
}

void string_iterate_lines(Vector const* strings, void (*closure)(char*))
{
    char** const lines = Vector_data(strings);
    for (size_t i = 0; i < Vector_size(strings); ++i)
        closure(lines[i]);
}

void string_iterate_lines_with_data(Vector const* strings, void (*closure)(char*, void*), void* extra)
{
    char** const lines = Vector_data(strings);
    for (size_t i = 0; i < Vector_size(strings); ++i)
        closure(lines[i], extra);
}

int string_length(char const* text)
{
    return strlen(text);
}

char* string_reverse(char const* palabra)
{
    char* resultado = Calloc(1, string_length(palabra) + 1);

    int i = string_length(palabra) - 1, j = 0;
    while (i >= 0)
    {
        resultado[j] = palabra[i];
        --i;
        ++j;
    }

    return resultado;
}

bool string_contains(char const* text, char const* substring)
{
    return strstr(text, substring) != NULL;
}

void string_replace(char* text, char viejo, char nuevo)
{
    for (char* i = text; *i != '\0'; ++i)
        if (*i == viejo)
            *i = nuevo;
}

/** PRIVATE FUNCTIONS **/

static void _string_upper_element(char* ch)
{
    *ch = toupper(*ch);
}

static void _string_lower_element(char* ch)
{
    *ch = tolower(*ch);
}

static void _string_do(char* text, void (* closure)(char* c))
{
    int i = 0;
    while (text[i] != '\0')
    {
        closure(&text[i]);
        i++;
    }
}


static void _string_append_with_format_list(const char* format, char** original, va_list arguments)
{
    va_list copy_arguments;
    va_copy(copy_arguments, arguments);
    size_t buffer_size = vsnprintf(NULL, 0, format, copy_arguments) + 1;
    va_end(copy_arguments);

    char temporal[buffer_size];
    va_copy(copy_arguments, arguments);
    vsnprintf(temporal, buffer_size, format, copy_arguments);
    va_end(copy_arguments);
    string_append(original, temporal);
}

static bool _is_last_token(char const* next, size_t index, size_t _)
{
    (void) index;
    (void) _;
    return !!*next;
}

static bool _is_last_n_token(char const* next, size_t index, size_t n)
{
    return !!*next && index + 1 < n;
}

static Vector _string_split(char const* text, char const* separator, bool(*condition)(char const*, size_t, size_t), size_t n)
{
    Vector substrings = VECTOR_OF_STRINGS_INITIALIZER;

    char* text_to_iterate = string_duplicate(text);

    char* next = text_to_iterate;
    char* str = text_to_iterate;

    while (condition(next, Vector_size(&substrings), n))
    {
        char* token = strtok_r(str, separator, &next);
        if (token == NULL)
            break;

        str = NULL;

        char* const elem = string_duplicate(token);
        Vector_push_back(&substrings, &elem);
    }

    if (next[0] != '\0')
    {
        char* const elem = string_duplicate(next);
        Vector_push_back(&substrings, &elem);
    }

    Free(text_to_iterate);
    return substrings;
}
