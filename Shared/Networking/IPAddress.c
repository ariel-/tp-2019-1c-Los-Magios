
#include "IPAddress.h"
#include "Logger.h"
#include <string.h>

void IPAddress_Init(IPAddress* ip, void const* sockAddr, socklen_t sockAddrLen)
{
    ip->Version = 0;
    ip->SockAddrLen = sockAddrLen;
    ip->HostIP[0] = '\0';
    ip->Port = 0;

    memcpy(&ip->IP, sockAddr, sockAddrLen);

    switch (ip->IP.ss_family)
    {
        case AF_INET:
            ip->Version = 4;
            inet_ntop(AF_INET, &ip->IPv4.sin_addr, ip->HostIP, INET_ADDRSTRLEN);
            ip->Port = ntohs(ip->IPv4.sin_port);
            break;
        case AF_INET6:
            ip->Version = 6;
            inet_ntop(AF_INET6, &ip->IPv6.sin6_addr, ip->HostIP, INET6_ADDRSTRLEN);
            ip->Port = ntohs(ip->IPv6.sin6_port);
            break;
        default:
            LISSANDRA_LOG_ERROR("IPAddress_Init: recibí 'ss_family' no soportado (%u)", ip->IP.ss_family);
            break;
    }
}
