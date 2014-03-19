#ifndef _HPIO_SOCKET_
#define _HPIO_SOCKET_

#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

#define SOCKADDRLEN PATH_MAX

enum {
    SK_NONBLOCK = 1,
    SK_NODELAY,
    SK_RECVTIMEOUT,
    SK_SENDTIMEOUT,
    SK_REUSEADDR,
};

int64_t sk_read(int sfd, char *buf, int64_t size);
int64_t sk_write(int sfd, const char *buf, int64_t size);
int sk_sockname(int sfd, char *sock, int size);
int sk_peername(int sfd, char *peer, int size);
int sk_connect(const char *net, const char *sock, const char *peer);
int sk_reconnect(int *sfd);
int sk_setopt(int sfd, int optname, ...);

int act_listen(const char *net, const char *sock, int backlog);
int act_accept(int afd);
int act_setopt(int afd, int optname, ...);




#endif
