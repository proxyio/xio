#ifndef _HPIO_XSOCK_
#define _HPIO_XSOCK_

#include <inttypes.h>

#define XPF_NET       1
#define XPF_IPC       2
#define XPF_INPROC    4

char *xallocmsg(int size);
void xfreemsg(char *xbuf);
int xmsglen(char *xbuf);

#define XLISTENER   1
#define XCONNECTOR  2
int xsocket(int pf, int type);

#define XSOCKADDRLEN 128
int xbind(int xd, const char *addr);
int xaccept(int xd);
int xrecv(int xd, char **xbuf);
int xsend(int xd, char *xbuf);
void xclose(int xd);

static inline int xlisten(int pf, const char *addr) {
    int xd = xsocket(pf, XLISTENER);

    if (xd >= 0 && xbind(xd, addr) == 0)
	return xd;
    return -1;
}

static inline int xconnect(int pf, const char *peer) {
    int xd = xsocket(pf, XCONNECTOR);

    if (xd >= 0 && xbind(xd, peer) == 0)
	return xd;
    return -1;
}

#define XNOBLOCK           0x0001
#define XSNDBUF            0x0002
#define XRCVBUF            0x0004
#define XLINGER            0x0008
#define XSNDTIMEO          0x0010
#define XRCVTIMEO          0x0020
#define XRECONNECT         0x0040
#define XRECONNECT_IVL     0x0080
#define XRECONNECT_IVLMAX  0x0100
#define XSOCKETKEY         0x0200


int xsetopt(int xd, int opt, void *val, int valsz);
int xgetopt(int xd, int opt, void *val, int valsz);


#endif
