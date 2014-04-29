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
#define XSNDBUF            1
#define XRCVBUF            2
#define XLINGER            3
#define XSNDTIMEO          4
#define XRCVTIMEO          5
#define XRECONNECT         6
#define XRECONNECT_IVL     7
#define XRECONNECT_IVLMAX  8
#define XSOCKTYPE          9
#define XSOCKPROTO         10
    
int xsetsockopt(int xd, int level, int optname, void *optval, int optlen);
int xgetsockopt(int xd, int level, int optname, void *optval, int *optlen);


#ifdef __cplusplus
}
#endif

#endif
