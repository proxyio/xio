#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "tcp.h"
#include "ds/list.h"

#define TP_TCP_BACKLOG 100


static struct transport tcp_transport_vfptr = {
    .name = "tcp",
    .proto = TP_TCP,

    .global_init = tcp_global_init,
    .close = tcp_close,
    .bind = tcp_bind,
    .accept = tcp_accept,
    .connect = tcp_connect,
    .read = tcp_read,
    .write = tcp_write,
    .setopt = tcp_setopt,
    .getopt = NULL,
    .item = LIST_ITEM_INITIALIZE,
};

struct transport *tcp_transport = &tcp_transport_vfptr;



void tcp_close(int fd) {
    close(fd);
}

int __unp_bind(const char *host, const char *serv) {
    int listenfd, n;
    const int on = 1;
    struct addrinfo hints, *res, *ressave;

    ZERO(hints);
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((n = getaddrinfo(host, serv, &hints, &res)) != 0)
	return -1;
    ressave = res;
    do {
	if ((listenfd = socket(res->ai_family, res->ai_socktype,
            res->ai_protocol)) < 0)
	    continue;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (bind(listenfd, res->ai_addr, res->ai_addrlen) == 0)
	    break;
	close(listenfd);
    } while ( (res = res->ai_next) != NULL);
    freeaddrinfo(ressave);
    return listenfd;
}

int tcp_bind(const char *sock) {
    int afd;
    char *host = NULL, *serv = NULL;

    if (!(serv = strrchr(sock, ':'))
	|| strlen(serv + 1) == 0 || !(host = strdup(sock)))
	return -1;
    host[serv - sock] = '\0';
    if (!(serv = strdup(serv + 1))) {
	free(host);
	return -1;
    }
    afd = __unp_bind(host, serv);
    free(host);
    free(serv);
    if (afd < 0 || listen(afd, TP_TCP_BACKLOG) < 0) {
	close(afd);
	return -1;
    }
    return afd;
}

int tcp_accept(int afd) {
    struct sockaddr_storage addr = {};
    socklen_t addrlen = sizeof(addr);
    int fd = accept(afd, (struct sockaddr *) &addr, &addrlen);

    if (fd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK
		   || errno == EINTR || errno == ECONNABORTED)) {
	errno = EAGAIN;
	return -1;
    }
    return fd;
}

int __unp_connect(const char *host, const char *serv) {
    int sockfd, n;
    struct addrinfo hints, *res, *ressave;

    ZERO(hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((n = getaddrinfo(host, serv, &hints, &res)) != 0)
	return -1;
    ressave = res;
    do {
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd < 0)
	    continue;
	if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0)
	    break;
	close(sockfd);
    } while ( (res = res->ai_next) != NULL);
    freeaddrinfo(res);
    return sockfd;
}

int tcp_connect(const char *peer) {
    int fd = 0;
    char *host = NULL, *serv = NULL;

    if (!(serv = strrchr(peer, ':'))
	|| strlen(serv + 1) == 0 || !(host = strdup(peer)))
	return -1;
    host[serv - peer] = '\0';
    if (!(serv = strdup(serv + 1))) {
	free(host);
	return -1;
    }
    fd = __unp_connect(host, serv);
    free(host);
    free(serv);
    return fd;
}


int64_t tcp_read(int sockfd, char *buf, int64_t len) {
    int64_t nbytes;
    nbytes = recv(sockfd, buf, len, 0);

    //  Several errors are OK. When speculative read is being done we
    //  may not be able to read a single byte to the socket. Also, SIGSTOP
    //  issued by a debugging tool can result in EINTR error.
    if (nbytes == -1 && (errno == EAGAIN ||
			 errno == EWOULDBLOCK || errno == EINTR)) {
	errno = EAGAIN;
        return -1;
    }
    //  Signalise peer failure.
    if (nbytes == 0) {
	errno = EPIPE;
	return -1;
    }
    return nbytes;
}

int64_t tcp_write(int sockfd, const char *buf, int64_t len) {
    int64_t nbytes;
    nbytes = send(sockfd, buf, len, 0);

    //  Several errors are OK. When speculative write is being done we
    //  may not be able to write a single byte to the socket. Also, SIGSTOP
    //  issued by a debugging tool can result in EINTR error.
    if (nbytes == -1 && (errno == EAGAIN ||
			 errno == EWOULDBLOCK || errno == EINTR)) {
	errno = EAGAIN;
        return -1;
    } else if (nbytes == -1) {
	// Signalise peer failure.
	errno = EPIPE;
	return -1;
    }
    return nbytes;
}

int tcp_sockname(int fd, char *sock, int size) {
    struct sockaddr_storage addr = {};
    socklen_t addrlen = sizeof(addr);
    struct sockaddr_in *sa_in;
    char tcp_addr[TP_SOCKADDRLEN] = {};

    if (-1 == getsockname(fd, (struct sockaddr *)&addr, &addrlen))
	return -1;
    sa_in = (struct sockaddr_in *)&addr;
    inet_ntop(AF_INET, (char *)&sa_in->sin_addr, tcp_addr, sizeof(tcp_addr));
    snprintf(tcp_addr + strlen(tcp_addr),
	     sizeof(tcp_addr) - strlen(tcp_addr), ":%d", ntohs(sa_in->sin_port));
    size = size > TP_SOCKADDRLEN - 1 ? TP_SOCKADDRLEN - 1 : size;
    sock[size] = '\0';
    memcpy(sock, tcp_addr, size);
    return 0;
}

int tcp_peername(int fd, char *peer, int size) {
    struct sockaddr_storage addr = {};
    socklen_t addrlen = sizeof(addr);
    struct sockaddr_in *sa_in;
    char tcp_addr[TP_SOCKADDRLEN] = {};

    if (-1 == getpeername(fd, (struct sockaddr *)&addr, &addrlen))
	return -1;
    sa_in = (struct sockaddr_in *)&addr;
    inet_ntop(AF_INET, (char *)&sa_in->sin_addr, tcp_addr, sizeof(tcp_addr));
    snprintf(tcp_addr + strlen(tcp_addr),
	     sizeof(tcp_addr) - strlen(tcp_addr), ":%d", ntohs(sa_in->sin_port));
    size = size > TP_SOCKADDRLEN - 1 ? TP_SOCKADDRLEN - 1 : size;
    peer[size] = '\0';
    memcpy(peer, tcp_addr, size);
    return 0;
}


int tcp_setopt(int fd, int opt, void *val, int valsz) {
    switch (opt) {
    case TP_SNDTIMEO:
    case TP_RCVTIMEO:
	{
	    int to = *((int *)val);
	    int ff = (opt == TP_SNDTIMEO) ? SO_SNDTIMEO : SO_RCVTIMEO;
	    struct timeval tv = {
		.tv_sec = to / 1000,
		.tv_usec = (to % 1000) * 1000,
	    };
	    return setsockopt(fd, SOL_SOCKET, ff, &tv, sizeof(tv));
	}
    case TP_NOBLOCK:
	{
	    int ff;
	    int block = *((int *)val);
	    if ((ff = fcntl(fd, F_GETFL, 0)) < 0)
		ff = 0;
	    ff = block ? (ff | O_NONBLOCK) : (ff & ~O_NONBLOCK);
	    return fcntl(fd, F_SETFL, ff);
	}
    case TP_NODELAY:
	{
	    int ff = *((int *)val) ? 1 : 0;
	    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &ff, sizeof(ff));
	}
    case TP_REUSEADDR:
	{
	    int ff = *((int *)val) ? 1 : 0;
	    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &ff, sizeof(ff));
	}
    }
    errno = EINVAL;
    return -1;
}
