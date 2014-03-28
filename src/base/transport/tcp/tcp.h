#ifndef _HPIO_SOCKET_
#define _HPIO_SOCKET_

#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

#define PIO_NONBLOCK 1
#define PIO_NODELAY 2
#define PIO_RCVTIMEO 3
#define PIO_SNDTIMEO 4
#define PIO_REUSEADDR 5

int tcp_bind(const char *sock);
int tcp_accept(int afd);

int tcp_connect(const char *peer);
int tcp_reconnect(int *sfd);
int tcp_setopt(int sfd, int optname, ...);

int64_t tcp_read(int sfd, char *buf, int64_t size);
int64_t tcp_write(int sfd, const char *buf, int64_t size);
int tcp_sockname(int sfd, char *sock, int size);
int tcp_peername(int sfd, char *peer, int size);



#endif
