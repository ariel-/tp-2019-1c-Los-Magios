
#ifndef Opcodes_h__
#define Opcodes_h__

#include "OpcodeHandler.h"

typedef struct Packet Packet;
typedef struct Socket Socket;

enum
{
    KERNEL = 1,
    MEMORIA = 2,
    LFS = 3
};

#define OPCODES_SENT(OPC) \
    /* Mensajes que entienden los 3 modulos */                                 \
    OPC(MSG_HANDSHAKE) /* mensaje enviado al conectar                          \
                        *                                                      \
                        * uint8: module id (ver enum)                          \
                        * -- mem y kernel->mem --                              \
                        * char* ip remota                                      \
                        * -- solo entre memorias --                            \
                        * uint32_t: id memoria                                 \
                        * char*: puerto escucha                                \
                        * sigue igual que MSG_GOSSIP_LIST                      \
                        *                                                      \
                        * Responde: MSG_HANDSHAKE_RESPUESTA (LFS->Memoria)     \
                        *           MSG_GOSSIP_LIST (Memoria->Kernel/Mem)      \
                        */                                                     \
                                                                               \
    /* requests */                                                             \
    OPC(LQL_SELECT)  /* char*: nombre tabla                                    \
                      * uint16: key                                            \
                      *                                                        \
                      * Responde: MSG_SELECT                                   \
                      */                                                       \
                                                                               \
    OPC(LQL_INSERT)  /* char*: nombre tabla                                    \
                      * uint16: key                                            \
                      * char*: value                                           \
                      * uint64: timestamp (de K->M no posee)                   \
                      */                                                       \
                                                                               \
    OPC(LQL_CREATE)  /* char*: nombre tabla                                    \
                      * uint8: tipo consistencia (ver Consistency.h)           \
                      * uint16: numero particiones                             \
                      * uint32: tiempo entre compactaciones, en milisegundos   \
                      */                                                       \
                                                                               \
    OPC(LQL_DESCRIBE) /* char* OPCIONAL: nombre tabla                          \
                       *                                                       \
                       * Responde: MSG_DESCRIBE o MSG_DESCRIBE_GLOBAL          \
                       */                                                      \
                                                                               \
    OPC(LQL_DROP)     /* char*: nombre tabla */                                \
                                                                               \
    /* Mensajes a memoria */                                                   \
    OPC(LQL_JOURNAL)        /* nada */                                         \

#define OPCODES_RECV(OPC)                                                      \
    /* Mensajes que entienden los 3 modulos                                    \
       (Respuestas a queries) */                                               \
    OPC(MSG_SELECT)  /* uint64: timestamp (FS->Mem solamente)                  \
                      * char*: value                                           \
                      */                                                       \
                                                                               \
    OPC(MSG_DESCRIBE) /* char*: nombre tabla                                   \
                       * uint8: tipo de consistencia (ver Consistency.h)       \
                       * uint16: numero de particiones                         \
                       * uint32: tiempo entre compactaciones, en milisegundos  \
                       */                                                      \
                                                                               \
    OPC(MSG_DESCRIBE_GLOBAL) /* uint32: cantidad de tablas                     \
                              * cada una se deserializa igual que MSG_DESCRIBE \
                              */                                               \
                                                                               \
                                                                               \
    OPC(MSG_HANDSHAKE_RESPUESTA) /* uint32: tamanioValue                       \
                                  * char*: puntoMontaje                        \
                                  */                                           \
                                                                               \
    OPC(MSG_CREATE_RESPUESTA)   /* uint8: EXIT_SUCCESS o EXIT_FAILURE          \
                                 */                                            \
                                                                               \
    OPC(MSG_DROP_RESPUESTA) /* uint8: EXIT_SUCCESS o EXIT_FAILURE              \
                             */                                                \
                                                                               \
    OPC(MSG_INSERT_RESPUESTA)   /* uint8: EXIT_SUCCESS o EXIT_FAILURE          \
                                 */                                            \
                                                                               \
    /* erores */                                                               \
    OPC(MSG_ERR_VALUE_TOO_LONG) /* valor demasiado largo */                    \
                                                                               \
    OPC(MSG_ERR_KEY_NOT_FOUND) /* key no encontrado */                         \
                                                                               \
    OPC(MSG_ERR_MEM_FULL)  /* memoria está full. Si es SELECT contiene:        \
                            *                                                  \
                            * char*: value                                     \
                            */                                                 \
                                                                               \
    OPC(MSG_ERR_TABLE_NOT_EXISTS) /* tabla no existe                           \
                                   */                                          \
                                                                               \
    /* gossiping */                                                            \
    OPC(MSG_GOSSIP_LIST)  /* uint32: cantidad de entradas                      \
                           *                                                   \
                           * uint32: id memoria                                \
                           * char*: ip memoria                                 \
                           * char*: puerto memoria                             \
                           */                                                  \

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(ENUM) #ENUM,

typedef enum
{
    OPCODES_SENT(GENERATE_ENUM)
    OPCODES_RECV(GENERATE_ENUM)
    NUM_OPCODES
} Opcodes;

#define NUM_HANDLED_OPCODES LQL_JOURNAL + 1

extern char const* OpcodeNames[NUM_OPCODES];

extern OpcodeHandlerFnType* const OpcodeTable[NUM_HANDLED_OPCODES];

#endif //Opcodes_h__
