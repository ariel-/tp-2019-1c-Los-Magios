
#include "Socket.h"
#include "ByteConverter.h"
#include "FDIImpl.h"
#include "Logger.h"
#include "Malloc.h"
#include "Opcodes.h"
#include "Packet.h"
#include <assert.h>
#include <netdb.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#pragma pack(push, 1)
typedef struct
{
    uint16_t size;
    uint16_t cmd;
} PacketHdr;
#pragma pack(pop)

static bool _acceptCb(void* socket);
static int _iterateAddrInfo(IPAddress* ip, struct addrinfo* ai, enum SocketMode mode);

typedef struct
{
    int fileDescriptor;
    IPAddress const* ipAddr;
    enum SocketMode mode;

    SocketAcceptFn* acceptFn;
} SockInit;
static Socket* _initSocket(SockInit const* si);

Socket* Socket_Create(SocketOpts const* opts)
{
    struct addrinfo hint = { 0 };
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    if (opts->SocketMode == SOCKET_SERVER) // listen socket
        hint.ai_flags = AI_PASSIVE;

    struct addrinfo* ai;
    int res = getaddrinfo(opts->HostName, opts->ServiceOrPort, &hint, &ai);
    if (res != 0)
    {
        LISSANDRA_LOG_DEBUG("Socket_Create: getaddrinfo devolvió %i (%s)", res, gai_strerror(res));
        return NULL;
    }

    IPAddress ip;
    int fd = _iterateAddrInfo(&ip, ai, opts->SocketMode);

    freeaddrinfo(ai);

    if (fd < 0)
    {
        LISSANDRA_LOG_DEBUG("Socket_Create: imposible conectar! HOST:SERVICIO = %s:%s", opts->HostName,
                            opts->ServiceOrPort);
        return NULL;
    }

    SockInit si =
    {
        .fileDescriptor = fd,
        .ipAddr = &ip,
        .mode = opts->SocketMode,

        .acceptFn = opts->SocketOnAcceptClient
    };
    return _initSocket(&si);
}

void Socket_SendPacket(Socket* s, Packet const* packet)
{
    uint16_t const opcode = Packet_GetOpcode(packet);
    uint16_t const packetSize = Packet_Size(packet);

    PacketHdr header =
    {
        .cmd = EndianConvert(opcode),
        .size = EndianConvert(packetSize)
    };

    LISSANDRA_LOG_TRACE("Enviando paquete a %s: %s (opcode: %u, tam: %u)", s->Address.HostIP, OpcodeNames[opcode], opcode, packetSize);

    // Enviar header+paquete en un solo send
    struct iovec iovecs[] =
    {
        { .iov_base = &header,                 .iov_len = sizeof header },
        { .iov_base = Packet_Contents(packet), .iov_len = packetSize    }
    };

    struct msghdr const hdr =
    {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = iovecs,
        .msg_iovlen = sizeof iovecs / sizeof *iovecs,
        .msg_control = NULL,
        .msg_controllen = 0,
        .msg_flags = 0
    };

    if (sendmsg(s->Handle, &hdr, MSG_NOSIGNAL | MSG_WAITALL) < 0)
        LISSANDRA_LOG_SYSERROR("sendmsg");
}

Packet* Socket_RecvPacket(Socket* s)
{
    PacketHdr header;
    ssize_t readLen = recv(s->Handle, &header, sizeof(PacketHdr), MSG_NOSIGNAL | MSG_WAITALL);
    if (readLen == 0)
    {
        // nos cerraron la conexion, limpiar socket
        return NULL;
    }

    if (readLen < 0)
    {
        // otro error, limpiar socket
        LISSANDRA_LOG_SYSERROR("recv");
        return NULL;
    }

    header.size = EndianConvert(header.size);
    header.cmd = EndianConvert(header.cmd);

    if (header.size >= 10240 || header.cmd >= NUM_OPCODES)
    {
        LISSANDRA_LOG_DEBUG("_readHeaderHandler(): Cliente %s ha enviado paquete no válido (tam: %hu, opc: %u)",
                            s->Address.HostIP, header.size, header.cmd);
        return NULL;
    }

    LISSANDRA_LOG_TRACE("Recibido paquete de %s: %s (opcode: %u, tam: %u)", s->Address.HostIP, OpcodeNames[header.cmd], header.cmd, header.size);

    // manejo especial de paquetes vacios (recv se queda bloqueado con size == 0)
    if (!header.size)
        return Packet_Create(header.cmd, 0);

    uint8_t* packetBuf = Malloc(header.size);
    readLen = recv(s->Handle, packetBuf, header.size, MSG_NOSIGNAL | MSG_WAITALL);
    if (readLen == 0)
    {
        // nos cerraron la conexion, limpiar socket
        return NULL;
    }

    if (readLen < 0)
    {
        // otro error
        LISSANDRA_LOG_SYSERROR("recv");
        return NULL;
    }

    return Packet_Adopt(header.cmd, packetBuf, header.size);
}

bool Socket_HandlePacket(void* socket)
{
    Socket* const s = socket;

    Packet* p = Socket_RecvPacket(s);
    if (!p)
        return false;

    uint16_t opc = Packet_GetOpcode(p);
    if (opc >= NUM_HANDLED_OPCODES)
    {
        LISSANDRA_LOG_DEBUG("Socket_HandlePacket: recibido opcode no soportado (%hu)", opc);
        return false;
    }

    OpcodeHandlerFnType* handler = OpcodeTable[opc];
    if (!handler)
    {
        LISSANDRA_LOG_DEBUG("Socket_HandlePacket: recibido paquete no soportado! (cmd: %hu)", opc);
        return false;
    }

    handler(s, p);
    Packet_Destroy(p);
    return true;
}

void Socket_Destroy(void* elem)
{
    Socket* const s = elem;

    close(s->Handle);
    Free(s);
}

static bool _acceptCb(void* socket)
{
    Socket* listener = socket;

    struct sockaddr_storage peerAddress;
    socklen_t saddr_len = sizeof peerAddress;
    int fd = accept(listener->Handle, (struct sockaddr*) &peerAddress, &saddr_len);
    if (fd < 0)
    {
        LISSANDRA_LOG_SYSERROR("accept");
        return true;
    }

    IPAddress ip;
    IPAddress_Init(&ip, &peerAddress, saddr_len);
    LISSANDRA_LOG_TRACE("Conexión desde %s:%u", ip.HostIP, ip.Port);

    SockInit si =
    {
        .fileDescriptor = fd,
        .ipAddr = &ip,
        .mode = SOCKET_CLIENT,

        .acceptFn = NULL
    };

    Socket* client = _initSocket(&si);
    if (listener->SockAcceptFn)
    {
        listener->SockAcceptFn(listener, client);
        return true;
    }

    EventDispatcher_AddFDI(client);
    return true;
}

static int _connectTimeout(int fd, struct sockaddr const* addr, socklen_t addrLen)
{
    static uint32_t const TIMEOUT_SECONDS = 5;

    // set non-blocking I/O
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;

    if (connect(fd, addr, addrLen) < 0)
    {
        // wait for the socket to connect, but up to a max of TIMEOUT_SECONDS
        if (errno == EINPROGRESS)
        {
            struct timeval tv =
            {
                .tv_sec = TIMEOUT_SECONDS,
                .tv_usec = 0
            };

            fd_set wait_set;
            FD_ZERO(&wait_set);
            FD_SET(fd, &wait_set);

            // when the socket is connected it shows as writable
            int res = select(fd + 1, NULL, &wait_set, NULL, &tv);
            if (res < 0)
                return -1;
            else if (res > 0)
            {
                int error;
                socklen_t len = sizeof(int);
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
                    return -1;

                if (error)
                {
                    errno = error;
                    return -1;
                }
            }
            else
            {
                // timeout
                errno = ETIMEDOUT;
                return -1;
            }
        }
        else
            return -1;
    }

    // restore blocking I/O
    if (fcntl(fd, F_SETFL, flags) < 0)
        return -1;

    return 0;
}

static int _iterateAddrInfo(IPAddress* ip, struct addrinfo* ai, enum SocketMode mode)
{
    for (struct addrinfo* i = ai; i != NULL; i = i->ai_next)
    {
        IPAddress_Init(ip, i->ai_addr, i->ai_addrlen);

        char const* address = ip->HostIP;
        uint16_t port = ip->Port;
        unsigned ipv = ip->Version;
        LISSANDRA_LOG_TRACE("Socket_Create: Intentado conectar a %s:%u (IPv%u)", address, port, ipv);
        int fd = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
        if (fd < 0)
        {
            LISSANDRA_LOG_SYSERROR("socket");
            continue;
        }
        else
            LISSANDRA_LOG_TRACE("Socket_Create: Creado socket en %s:%u (IPv%u)", address, port, ipv);

        if (mode == SOCKET_SERVER)
        {
            int yes = 1;
            if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) < 0)
            {
                close(fd);
                LISSANDRA_LOG_SYSERROR("setsockopt");
                continue;
            }
        }

        int(*tryFn)(int, struct sockaddr const*, socklen_t) = (mode == SOCKET_CLIENT ? _connectTimeout : bind);
        if (tryFn(fd, (struct sockaddr const*) &ip->IP, ip->SockAddrLen) < 0)
        {
            close(fd);
            LISSANDRA_LOG_SYSERROR(mode == SOCKET_CLIENT ? "connect" : "bind");
            continue;
        }

        char const* msg = mode == SOCKET_CLIENT ? "Conectado" : "Asociado";
        LISSANDRA_LOG_TRACE("Socket_Create: %s a %s:%u! (IPv%u)", msg, address, port, ipv);

        if (mode == SOCKET_SERVER)
        {
            if (listen(fd, BACKLOG) < 0)
            {
                close(fd);
                LISSANDRA_LOG_SYSERROR("listen");
                continue;
            }

            LISSANDRA_LOG_TRACE("Socket_Create: Escuchando conexiones entrantes en %s:%u (IPv%u)", address, port, ipv);
        }
        return fd;
    }

    // si llegamos acá es porque fallamos en algo
    return -1;
}

static Socket* _initSocket(SockInit const* si)
{
    Socket* s = Malloc(sizeof(Socket));
    s->Handle = si->fileDescriptor;
    s->ReadCallback = NULL;
    s->_destroy = Socket_Destroy;

    s->SockAcceptFn = si->acceptFn;
    s->Address = *si->ipAddr;

    switch (si->mode)
    {
        case SOCKET_SERVER:
            s->ReadCallback = _acceptCb;
            break;
        case SOCKET_CLIENT:
            s->ReadCallback = Socket_HandlePacket;
            break;
        default:
            break;
    }

    return s;
}
