#ifndef _HPIO_TCP_
#define _HPIO_TCP_

#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

#define PIO_LINGER 1
#define PIO_SNDBUF 2
#define PIO_RCVBUF 3
#define PIO_NONBLOCK 4
#define PIO_NODELAY 5
#define PIO_RCVTIMEO 6
#define PIO_SNDTIMEO 7
#define PIO_REUSEADDR 8
#define PIO_SOCKADDRLEN 4096


void tcp_close(int fd);
int tcp_bind(const char *sock);
int tcp_accept(int fd);
int tcp_connect(const char *peer);

int tcp_setopt(int fd, int opt, void *val, int vallen);
int tcp_getopt(int fd, int opt, void *val, int vallen);

int64_t tcp_read(int fd, char *buff, int64_t size);
int64_t tcp_write(int fd, const char *buff, int64_t size);
int tcp_sockname(int fd, char *sockname, int size);
int tcp_peername(int fd, char *peername, int size);



#endif
