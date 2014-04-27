#ifndef _HPIO_XSOCK_
#define _HPIO_XSOCK_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

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


/* Following sockopt-level are provided by xsocket */
#define XL_SOCKET          1
#define XL_TRANSPORT       2

/* Following sockopt-field are provided by xsocket */
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

int xsetsockopt(int xd, int level, int optname, void *optval, int optlen);
int xgetsockopt(int xd, int level, int optname, void *optval, int *optlen);


#ifdef __cplusplus
}
#endif

#endif
