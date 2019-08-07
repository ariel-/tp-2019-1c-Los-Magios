
#ifndef IPAddress_h__
#define IPAddress_h__

#include <inttypes.h>
#include <arpa/inet.h>

typedef struct
{
    uint8_t Version;
    socklen_t SockAddrLen;
    char HostIP[INET6_ADDRSTRLEN];
    uint16_t Port;

    union
    {
        struct sockaddr_storage IP;
        struct sockaddr_in6     IPv6;
        struct sockaddr_in      IPv4;
    };
} IPAddress;

void IPAddress_Init(IPAddress* ip, void const* sockAddr, socklen_t sockAddrLen);

#endif //IPAddress_h__
