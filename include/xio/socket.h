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

#define XLISTENER   1
#define XCONNECTOR  2
    
/* Following sockopt-level are provided by xsocket */
#define XL_SOCKET          1

/* Following sockopt-field are provided by xsocket */
#define XNOBLOCK           0
#define XSNDWIN            1
#define XRCVWIN            2
#define XSNDBUF            3
#define XRCVBUF            4
#define XLINGER            5
#define XSNDTIMEO          6
#define XRCVTIMEO          7
#define XRECONNECT         8
#define XRECONNECT_IVL     9
#define XRECONNECT_IVLMAX  10
#define XSOCKTYPE          11
#define XSOCKPROTO         12
#define XTRACEDEBUG        13
    
int xsetsockopt(int xd, int level, int optname, void *optval, int optlen);
int xgetsockopt(int xd, int level, int optname, void *optval, int *optlen);


#ifdef __cplusplus
}
#endif

#endif
