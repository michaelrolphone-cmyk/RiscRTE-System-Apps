#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
int diagnosticGetSockName(int, sockaddr *, socklen_t *);
int diagnosticGetPeerName(int, sockaddr *, socklen_t *);
#define getsockname diagnosticGetSockName
#define getpeername diagnosticGetPeerName
