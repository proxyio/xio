#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include "ipc.h"
#include "ds/list.h"

#define TP_IPC_SOCKDIR "/tmp/proxyio"
#define TP_IPC_BACKLOG 100

static struct transport ipc_transport_vfptr = {
    .name = "ipc",
    .proto = TP_IPC,

    .global_init = ipc_global_init,
    .close = ipc_close,
    .bind = ipc_bind,
    .accept = ipc_accept,
    .connect = ipc_connect,
    .read = ipc_read,
    .write = ipc_write,
    .setopt = ipc_setopt,
    .getopt = NULL,
    .item = LIST_ITEM_INITIALIZE,
};

struct transport *ipc_transport = &ipc_transport_vfptr;


void ipc_close(int fd) {
    struct sockaddr_un addr = {};

    ipc_sockname(fd, addr.sun_path, sizeof(addr.sun_path));
    close(fd);
    if (strlen(addr.sun_path) > 0)
	unlink(addr.sun_path);
}


int ipc_bind(const char *sock) {
    int afd;
    struct sockaddr_un addr;
    socklen_t addr_len = sizeof(addr);
    
    ZERO(addr);
    if ((afd = socket(AF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0)) < 0)
	return -1;
    addr.sun_family = AF_LOCAL;
    snprintf(addr.sun_path,
	     sizeof(addr.sun_path), "%s/%s", TP_IPC_SOCKDIR, sock);
    unlink(addr.sun_path);
    if (bind(afd, (struct sockaddr *)&addr, addr_len) < 0
	|| listen(afd, TP_IPC_BACKLOG) < 0) {
	close(afd);
	return -1;
    }
    return afd;
}

int ipc_accept(int afd) {
    struct sockaddr_un addr = {};
    socklen_t addrlen = sizeof(addr);
    int fd = accept(afd, (struct sockaddr *) &addr, &addrlen);
    
    if (fd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR
		   || errno == ECONNABORTED || errno == EMFILE)) {
	errno = EAGAIN;
	return -1;
    }
    return fd;
}

int ipc_connect(const char *peer) {
    int fd;
    struct sockaddr_un addr;
    socklen_t addr_len = sizeof(addr);

    ZERO(addr);
    if ((fd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0)
	return -1;
    addr.sun_family = AF_LOCAL;
    snprintf(addr.sun_path,
	     sizeof(addr.sun_path), "%s/%s", TP_IPC_SOCKDIR, peer);
    if (connect(fd, (struct sockaddr *)&addr, addr_len) < 0) {
	close(fd);
	return -1;
    }
    return fd;
}

int64_t ipc_read(int sockfd, char *buf, int64_t len) {
    int64_t nbytes = recv(sockfd, buf, len, 0);

    if (nbytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
	errno = EAGAIN;
        return -1;
    } else if (nbytes == 0) {    //  Signalise peer failure.
	errno = EPIPE;
	return -1;
    }
    return nbytes;
}

int64_t ipc_write(int sockfd, const char *buf, int64_t len) {
    int64_t nbytes = send(sockfd, buf, len, 0);

    if (nbytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
	errno = EAGAIN;
        return -1;
    } else if (nbytes == -1) {
	// Signalise peer failure.
	errno = EPIPE;
	return -1;
    }
    return nbytes;
}


/* TODO:
 * getsockname/getpeername doesn't support unix domain socket.
 */

int ipc_sockname(int fd, char *sock, int size) {
    struct sockaddr_un addr = {};
    socklen_t addrlen = sizeof(addr);

    if (-1 == getsockname(fd, (struct sockaddr *)&addr, &addrlen))
	return -1;
    if (addrlen != sizeof(addr)) {
	errno = EINVAL;
	return -1;
    }
    size = size > strlen(addr.sun_path) ? strlen(addr.sun_path) : size;
    strncpy(sock, addr.sun_path, size);
    return 0;
}

int ipc_peername(int fd, char *peer, int size) {
    struct sockaddr_un addr = {};
    socklen_t addrlen = sizeof(addr);

    if (-1 == getpeername(fd, (struct sockaddr *)&addr, &addrlen))
	return -1;
    if (addrlen != sizeof(addr)) {
	errno = EINVAL;
	return -1;
    }
    size = size > strlen(addr.sun_path) ? strlen(addr.sun_path) : size;
    strncpy(peer, addr.sun_path, size);
    return 0;
}


int ipc_setopt(int fd, int opt, void *val, int valsz) {
    switch (opt) {
    case TP_NOBLOCK:
	{
	    int ff;
	    int block = *((int *)val);
	    if ((ff = fcntl(fd, F_GETFL, 0)) < 0)
		ff = 0;
	    ff = block ? (ff | O_NONBLOCK) : (ff & ~O_NONBLOCK);
	    return fcntl(fd, F_SETFL, ff);
	}
    }
    errno = EINVAL;
    return -1;
}
