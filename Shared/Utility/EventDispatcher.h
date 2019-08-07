
#ifndef EventDispatcher_h__
#define EventDispatcher_h__

#include <stdbool.h>

/*
 * Inicializa el EventDispatcher
 */
bool EventDispatcher_Init(void);

/*
 * Añade un nuevo item que implemente la interfaz de abstracciones de File Descriptors
 * Para que sus callback sean llamados
 */
void EventDispatcher_AddFDI(void* interface);

/*
 * Quita un item de la lista activa, sus eventos serán ignorados
 */
void EventDispatcher_RemoveFDI(void* interface);

/*
 * Chequea la existencia de eventos, en cuyo caso invoca los callback correspondientes
 */
void EventDispatcher_Dispatch(void);

/*
 * Main loop de los procesos
 */
void EventDispatcher_Loop(void);

/*
 * Destruye el EventDispatcher
 */
void EventDispatcher_Terminate(void);

#endif //EventDispatcher_h__
