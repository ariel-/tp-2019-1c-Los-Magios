
#ifndef LISSANDRA_COMPACTADOR_H
#define LISSANDRA_COMPACTADOR_H

#include <stdint.h>

//funciones para manejo Compactacion
void inicializarCompactador(void);

void agregarTablaCompactador(char const* nombreTabla, uint32_t tiempoCompactaciones);

void quitarTablaCompactador(char const* nombreTabla);

void compactar(char* nombreTabla);

void terminarCompactador(void);

#endif //LISSANDRA_COMPACTADOR_H
