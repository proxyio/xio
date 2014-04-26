#ifndef _HPIO_XSOCK_
#define _HPIO_XSOCK_

#include <inttypes.h>

char *xallocmsg(int size);
void xfreemsg(char *xbuf);
int xmsglen(char *xbuf);

#define XSOCKADDRLEN 128
int xlisten(const char *addr);
int xaccept(int xd);
int xconnect(const char *peer);

int xrecv(int xd, char **xbuf);
int xsend(int xd, char *xbuf);
void xclose(int xd);



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
