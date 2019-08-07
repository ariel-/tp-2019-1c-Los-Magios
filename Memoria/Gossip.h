
#ifndef Gossip_h__
#define Gossip_h__

#include <stdint.h>

typedef struct PeriodicTimer PeriodicTimer;
typedef struct Socket Socket;

void Gossip_Init(PeriodicTimer* gossipTimer);

void Gossip_AddMemory(uint32_t memId, char const* memIp, char const* memPort);

void Gossip_Do(PeriodicTimer* pt);

void Gossip_SendTable(Socket* peer);

void Gossip_Terminate(void);

#endif //Gossip_h__
