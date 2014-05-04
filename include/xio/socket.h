#ifndef _HPIO_XSOCK_
#define _HPIO_XSOCK_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

char *xallocmsg(int size);
void xfreemsg(char *xmsg);
int xmsglen(char *xmsg);

#define XMSG_OOBNUM      0
#define XMSG_GETOOB      1
#define XMSG_SETOOB      2
#define XMSG_CLONE       3
#define XMSG_COPYOOB     4
#define XMSG_SWITCHOOB   5


struct xmsgoob {
    uint8_t pos;
    char *outofband;
};    
    
int xmsgctl(char *xmsg, int opt, void *optval);

/* Following address family are provided */
#define XPF_TCP        1
#define XPF_IPC        2
#define XPF_INPROC     4

/* Following scalability address family are provided */
#define XPF_SP         8

/* Following socktype are provided */
#define XLISTENER       1
#define XCONNECTOR      2

/* Following scalability socktype are provided */
#define XSP_LISTENER    3
#define XSP_ENDPOINT    4

int xsocket(int pf, int socktype);
int xbind(int fd, const char *addr);


#define XSOCKADDRLEN 128
int xlisten(const char *addr);
int xaccept(int fd);
int xconnect(const char *peer);

int xrecv(int fd, char **xmsg);
int xsend(int fd, char *xmsg);
void xclose(int fd);

/* Following sockopt-level are provided */
#define XL_SOCKET          1

/* Following sockopt-field are provided */
#define XNOBLOCK           0
#define XSNDWIN            1
#define XRCVWIN            2
#define XSNDBUF            3
#define XRCVBUF            4
#define XLINGER            5
#define XSNDTIMEO          6
#define XRCVTIMEO          7
#define XRECONNECT         8
#define XSOCKTYPE          9
#define XSOCKPROTO         10
#define XTRACEDEBUG        11
    
int xsetopt(int fd, int level, int opt, void *optval, int optlen);
int xgetopt(int fd, int level, int opt, void *optval, int *optlen);


#ifdef __cplusplus
}
#endif

#endif
