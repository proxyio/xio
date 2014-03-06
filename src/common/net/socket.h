#ifndef _H_SOCKET_
#define _H_SOCKET_

#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

#define SOCKADDRLEN PATH_MAX

int64_t sk_read(int sfd, char *buf, int64_t size);
int64_t sk_write(int sfd, const char *buf, int64_t size);
int sk_sockname(int sfd, char *sock, int size);
int sk_peername(int sfd, char *peer, int size);
int sk_connect(const char *net, const char *sock, const char *peer);
int sk_reconnect(int *sfd);
int sk_setopt(int sfd, int optname, ...);


#endif
