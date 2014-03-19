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
#include "base.h"
#include "tcp.h"


int __unp_listen(const char *host, const char *serv) {
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
	if ((listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
	    continue;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (bind(listenfd, res->ai_addr, res->ai_addrlen) == 0)
	    break;
	close(listenfd);
    } while ( (res = res->ai_next) != NULL);
    freeaddrinfo(ressave);
    return listenfd;
}

static int __tcp_listen(const char *sock, int backlog) {
    int afd;
    char *host = NULL, *serv = NULL;

    if (!(serv = strrchr(sock, ':')) || strlen(serv + 1) == 0 || !(host = strdup(sock)))
	return -1;
    host[serv - sock] = '\0';
    if (!(serv = strdup(serv + 1))) {
	free(host);
	return -1;
    }
    afd = __unp_listen(host, serv);
    free(host);
    free(serv);
    if (afd < 0 || listen(afd, backlog) < 0) {
	close(afd);
	return -1;
    }
    return afd;
}


int tcp_listen(const char *net, const char *sock, int backlog) {
    if (STREQ(net, "") || (!STREQ(net, "tcp") && !STREQ(net, "tcp4") && !STREQ(net, "tcp6"))) {
	errno = EINVAL;
	return -1;
    }
    return __tcp_listen(sock, backlog);
}

int tcp_accept(int afd) {
    int sfd;
    struct sockaddr_storage addr = {};
    socklen_t addrlen = sizeof(addr);

    if ((sfd = accept(afd, (struct sockaddr *) &addr, &addrlen)) < 0 &&
	(errno == EAGAIN || errno == EWOULDBLOCK ||  errno == EINTR || errno == ECONNABORTED)) {
	errno = EAGAIN;
	return -1;
    }

    // fixed for other errno, like EMFILE, ENFILE, ENOBUFS, ENOMEM.
    return sfd;
}

static inline int tcp_set_block(int afd, int nonblock) {
    int flags = 0;

    flags = fcntl(afd, F_GETFL, 0);
    if (flags == -1)
	flags = 0;
    if (nonblock)
	flags |= O_NONBLOCK;
    else
	flags &= ~O_NONBLOCK;
    return fcntl(afd, F_SETFL, flags);
}

static inline int tcp_set_reuseaddr(int afd, int reuse) {
    reuse = reuse == true ? 1 : 0;
    return setsockopt(afd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
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
	if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
	    continue;
	if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0)
	    break;
	close(sockfd);
    } while ( (res = res->ai_next) != NULL);
    freeaddrinfo(res);
    return sockfd;
}

static int __tcp_connect(const char *sock, const char *peer) {
    int sfd = 0;
    char *host = NULL, *serv = NULL;

    if (!(serv = strrchr(peer, ':')) || strlen(serv + 1) == 0 || !(host = strdup(peer)))
	return -1;
    host[serv - peer] = '\0';
    if (!(serv = strdup(serv + 1))) {
	free(host);
	return -1;
    }
    sfd = __unp_connect(host, serv);
    free(host);
    free(serv);
    return sfd;
}


// tcp_connect() connects to the remote address raddr on the network net,
// which must be "tcp", "tcp4", or "tcp6".  If laddr is not nil, it is
// used as the local address for the connection.

int tcp_connect(const char *net, const char *sock, const char *peer) {
    if (STREQ(net, "") || (!STREQ(net, "tcp") && !STREQ(net, "tcp4") && !STREQ(net, "tcp6"))) {
	errno = EINVAL;
	return -1;
    }
    return __tcp_connect(sock, peer);
}


int64_t tcp_read(int sockfd, char *buf, int64_t len) {
    int64_t nbytes;
    nbytes = recv(sockfd, buf, len, 0);


    //  Several errors are OK. When speculative read is being done we may not
    //  be able to read a single byte to the socket. Also, SIGSTOP issued
    //  by a debugging tool can result in EINTR error.
    if (nbytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
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

    //  Several errors are OK. When speculative write is being done we may not
    //  be able to write a single byte to the socket. Also, SIGSTOP issued
    //  by a debugging tool can result in EINTR error.
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

int tcp_sockname(int sfd, char *sock, int size) {
    struct sockaddr_storage addr = {};
    socklen_t addrlen = sizeof(addr);
    struct sockaddr_in *sa_in;
    char tcp_addr[SOCKADDRLEN] = {};

    if (-1 == getsockname(sfd, (struct sockaddr *)&addr, &addrlen))
	return -1;
    sa_in = (struct sockaddr_in *)&addr;
    inet_ntop(AF_INET, (char *)&sa_in->sin_addr, tcp_addr, sizeof(tcp_addr));
    snprintf(tcp_addr + strlen(tcp_addr),
	     sizeof(tcp_addr) - strlen(tcp_addr), ":%d", ntohs(sa_in->sin_port));
    size = size > SOCKADDRLEN - 1 ? SOCKADDRLEN - 1 : size;
    sock[size] = '\0';
    memcpy(sock, tcp_addr, size);
    return 0;
}

int tcp_peername(int sfd, char *peer, int size) {
    struct sockaddr_storage addr = {};
    socklen_t addrlen = sizeof(addr);
    struct sockaddr_in *sa_in;
    char tcp_addr[SOCKADDRLEN] = {};

    if (-1 == getpeername(sfd, (struct sockaddr *)&addr, &addrlen))
	return -1;
    sa_in = (struct sockaddr_in *)&addr;
    inet_ntop(AF_INET, (char *)&sa_in->sin_addr, tcp_addr, sizeof(tcp_addr));
    snprintf(tcp_addr + strlen(tcp_addr),
	     sizeof(tcp_addr) - strlen(tcp_addr), ":%d", ntohs(sa_in->sin_port));
    size = size > SOCKADDRLEN - 1 ? SOCKADDRLEN - 1 : size;
    peer[size] = '\0';
    memcpy(peer, tcp_addr, size);
    return 0;
}


int tcp_reconnect(int *sfd) {
    int nfd;
    char sock[SOCKADDRLEN] = {}, peer[SOCKADDRLEN] = {};

    if (tcp_sockname(*sfd, sock, SOCKADDRLEN) < 0 || tcp_peername(*sfd, peer, SOCKADDRLEN) < 0)
	return -1;
    if ((nfd = __tcp_connect(sock, peer)) < 0)
	return -1;
    close(*sfd);
    *sfd = nfd;
    return 0;
}


int tcp_setopt(int sfd, int optname, ...) {
    va_list ap;

    switch (optname) {
    case PIO_TCP_SENDTIMEOUT:
    case PIO_TCP_RECVTIMEOUT:
	{
	    int to_msec;
	    int ff = (optname == PIO_TCP_SENDTIMEOUT) ? SO_SNDTIMEO : SO_RCVTIMEO;
	    struct timeval to;
	    va_start(ap, optname);
	    to_msec = va_arg(ap, int) / 1000;
	    va_end(ap);
	    to.tv_sec = to_msec / 1000;
	    to.tv_usec = (to_msec - to.tv_sec * 1000) * 1000;
	    return setsockopt(sfd, SOL_SOCKET, ff, (char *)&to, sizeof(to));
	}
    case PIO_TCP_NONBLOCK:
	{
	    int ff, block;
	    va_start(ap, optname);
	    block = va_arg(ap, int);
	    va_end(ap);
	    if ((ff = fcntl(sfd, F_GETFL, 0)) < 0)
		ff = 0;
	    ff = block ? (ff | O_NONBLOCK) : (ff & ~O_NONBLOCK);
	    return fcntl(sfd, F_SETFL, ff);
	}
    case PIO_TCP_NODELAY:
	{
	    int ff, delay;
	    va_start(ap, optname);
	    delay = va_arg(ap, int);
	    va_end(ap);
	    ff = delay ? true : false;
	    return setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, (char *)&ff, sizeof(ff));
	}
    case PIO_TCP_REUSEADDR:
	{
	    return tcp_set_reuseaddr(sfd, optname);
	}
    }
    errno = EINVAL;
    return -1;
}
