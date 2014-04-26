#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sync/waitgroup.h>
#include <runner/taskpool.h>
#include <transport/sockaddr.h>
#include "xsock_struct.h"

int xconnect(const char *peer) {
    int pf = sockaddr_pf(peer);
    int xd = xsocket(pf, XCONNECTOR);
    char sx_addr[TP_SOCKADDRLEN] = {};

    if (xd < 0 ||
	sockaddr_addr(peer, sx_addr, sizeof(sx_addr)) != 0 ||
	xbind(xd, sx_addr) != 0)
	return -1;
    return xd;
}
