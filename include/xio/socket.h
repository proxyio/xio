#ifndef _HPIO_XSOCK_
#define _HPIO_XSOCK_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

char *xallocmsg(int size);
void xfreemsg(char *xmsg);
int xmsglen(char *xmsg);

#define XMSG_OOBCNT    0
#define XMSG_GETOOB    1
#define XMSG_SETOOB    2

struct xmsgoob {
    int8_t pos;
    char *outofband;
};    
    
int xmsgctl(char *xmsg, int opt, void *optval);

    
#define XSOCKADDRLEN 128
int xlisten(const char *addr);
int xaccept(int xd);
int xconnect(const char *peer);

int xrecv(int xd, char **xmsg);
int xsend(int xd, char *xmsg);
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
    
int xsetopt(int xd, int level, int opt, void *optval, int optlen);
int xgetopt(int xd, int level, int opt, void *optval, int *optlen);


#ifdef __cplusplus
}
#endif

#endif
