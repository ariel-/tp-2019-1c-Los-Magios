
#ifndef Socket_h__
#define Socket_h__

#include "FileDescInterface.h"
#include "IPAddress.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Socket Socket;
typedef struct Packet Packet;

typedef void SocketAcceptFn(Socket* s, Socket* client);

typedef struct Socket
{
    FDI _impl;

    SocketAcceptFn* SockAcceptFn;
    IPAddress Address;
} Socket;

typedef struct
{
    char const* HostName;
    char const* ServiceOrPort;

    enum SocketMode
    {
        SOCKET_CLIENT,
        SOCKET_SERVER
    } SocketMode;

    SocketAcceptFn* SocketOnAcceptClient;
} SocketOpts;

#define BACKLOG 10

/*
 * Socket_Create: crea un nuevo Socket
 * Parámetros: estructura opts, con las opciones del mismo
 *  - HostName: string - ip o nombre del remoto, para servidores se puede pasar NULL
 *  - ServiceOrPort: string - puerto al que conectar, o en el cual aceptar conexiones si soy servidor
 *      También puede ponerse como string uno de los servicios registados en /etc/services
 *  - SocketMode: enum - O bien SOCKET_CLIENT o bien SOCKET_SERVER, dependiendo de lo que querramos
 *      el modo SOCKET_CLIENT inicia la conexión al remoto
 *      el modo SOCKET_SERVER acepta conexiones entrantes en el puerto pasado por parámetro
 */
Socket* Socket_Create(SocketOpts const* opts);

/*
 * Socket_SendPacket: envía un paquete serializado al host conectado
 */
void Socket_SendPacket(Socket* s, Packet const* packet);

/*
 * Socket_RecvPacket: recibe un paquete serializado
 */
Packet* Socket_RecvPacket(Socket* s);

/*
 * Socket_HandlePacket: recibe un paquete serializado y además llama al manejador correspondiente,
 * registrado en OpcodeTable
 *
 * Devuelve false si hubo algun error
 */
bool Socket_HandlePacket(void* s);

/*
 * Socket_Destroy: destructor, llamar luego de terminar de operar con el socket para liberar memoria
 */
void Socket_Destroy(void* elem);

#endif //Socket_h__
