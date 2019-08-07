
#ifndef LISSANDRA_LISSANDRALIBRARY_H
#define LISSANDRA_LISSANDRALIBRARY_H

#include "Memtable.h"
#include <libcommons/list.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <Timer.h>

typedef struct
{
    char table[NAME_MAX + 1];
    uint8_t consistency;
    uint16_t partitions;
    uint32_t compaction_time;
} t_describe;

void iniciar_servidor(void);

void mkdirRecursivo(char const* path);

bool existeArchivo(char const* path);

bool existeDir(char const* pathDir);

void generarPathTabla(char* nombreTabla, char* buf);

bool buscarBloqueLibre(size_t* bloqueLibre);

void generarPathBloque(size_t numBloque, char* buf);

void escribirValorBitarray(bool valor, size_t pos);

bool get_table_metadata(char const* tabla, t_describe* res);

int traverse(char const* fn, t_list* lista, char const* tabla);

bool dirIsEmpty(char const* path);

bool isLFSFile(char const* nombreArchivo);

void generarPathArchivo(char const* nombreTabla, char const* nombreArchivo, char* buf);

int traverse_to_drop(char const* pathTabla);

uint16_t get_particion(uint16_t particiones, uint16_t key);

void generarPathParticion(uint16_t particion, char* pathTabla, char* pathParticion);

bool get_biggest_timestamp(char const* contenido, uint16_t key, t_registro* resultado);

bool scanParticion(char const* pathParticion, uint16_t key, t_registro* registro);

bool temporales_get_biggest_timestamp(char const* pathTabla, uint16_t key, t_registro* registro);

t_registro const* get_newest(t_registro const* particion, t_registro const* temporales, t_registro const* memtable);

// primitivas FS
char* leerArchivoLFS(char const* path);

void escribirArchivoLFS(char const* path, char const* buf, size_t len);

void crearArchivoLFS(char const* path, size_t block);

void borrarArchivoLFS(char const* pathArchivo);

#endif //LISSANDRA_LISSANDRALIBRARY_H
