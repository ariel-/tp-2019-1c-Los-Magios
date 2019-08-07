
#ifndef LISSANDRA_MEMTABLE_H
#define LISSANDRA_MEMTABLE_H

#include <stdint.h>
#include <vector.h>

typedef struct PeriodicTimer PeriodicTimer;

typedef struct
{
    uint64_t timestamp;
    uint16_t key;
    char value[];
} t_registro;

#define REGISTRO_SIZE sizeof(t_registro) + confLFS.TAMANIO_VALUE + 1

void memtable_create(void);

//Funcion para meterle nuevos elementos a la memtable
void memtable_new_elem(char const* nombreTabla, uint16_t key, char const* value, uint64_t timestamp);

//Funcion para buscar segun una key dada el registro con mayor timestamp
bool memtable_get_biggest_timestamp(char const* nombreTabla, uint16_t key, t_registro* resultado);

//Funcion para eliminar un elemento de la memtable
void memtable_delete_table(char const* nombreTabla);

void* memtable_dump_thread(void*);

void memtable_destroy(void);

#endif //LISSANDRA_MEMTABLE_H
