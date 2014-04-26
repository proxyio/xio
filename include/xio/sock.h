#ifndef _HPIO_X
#define _HPIO_X

#include <inttypes.h>

#define PF_NET       1  /* TP_TCP */
#define PF_IPC       2  /* TP_IPC */
#define PF_INPROC    4  /* TP_MOCK_INPROC */

char *xallocmsg(uint32_t size);
void xfreemsg(char *xbuf);
uint32_t xmsglen(char *xbuf);

#define XLISTENER   1
#define XCONNECTOR  2
int xsocket(int pf, int type);
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

#define XNOBLOCK 1
#define XSNDBUF  2
#define XRCVBUF  4

int xsetopt(int xd, int opt, void *val, int valsz);
int xgetopt(int xd, int opt, void *val, int valsz);


#endif
