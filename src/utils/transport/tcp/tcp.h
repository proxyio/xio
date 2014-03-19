#ifndef _HPIO_SOCKET_
#define _HPIO_SOCKET_

#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

#define SOCKADDRLEN PATH_MAX

enum {
    PIO_TCP_NONBLOCK = 1,
    PIO_TCP_NODELAY,
    PIO_TCP_RECVTIMEOUT,
    PIO_TCP_SENDTIMEOUT,
    PIO_TCP_REUSEADDR,
};

int tcp_listen(const char *net, const char *sock, int backlog);
int tcp_accept(int afd);

int tcp_connect(const char *net, const char *sock, const char *peer);
int tcp_reconnect(int *sfd);
int tcp_setopt(int sfd, int optname, ...);

int64_t tcp_read(int sfd, char *buf, int64_t size);
int64_t tcp_write(int sfd, const char *buf, int64_t size);
int tcp_sockname(int sfd, char *sock, int size);
int tcp_peername(int sfd, char *peer, int size);





#endif
