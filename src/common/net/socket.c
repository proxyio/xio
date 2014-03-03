#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "base.h"
#include "socket.h"

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


// sk_connect() connects to the remote address raddr on the network net,
// which must be "tcp", "tcp4", or "tcp6".  If laddr is not nil, it is
// used as the local address for the connection.

int sk_connect(const char *net, const char *sock, const char *peer) {
    if (STREQ(net, "") || (!STREQ(net, "tcp") && !STREQ(net, "tcp4") && !STREQ(net, "tcp6"))) {
	errno = EINVAL;
	return -1;
    }
    return __tcp_connect(sock, peer);
}


int64_t sk_read(int sockfd, char *buf, int64_t len) {
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

int64_t sk_write(int sockfd, const char *buf, int64_t len) {
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

int sk_sockname(int sfd, char *sock, int size) {
    struct sockaddr_storage addr = {};
    socklen_t addrlen = sizeof(addr);
    struct sockaddr_in *sa_in;
    char sk_addr[SOCKADDRLEN] = {};

    if (-1 == getsockname(sfd, (struct sockaddr *)&addr, &addrlen))
	return -1;
    sa_in = (struct sockaddr_in *)&addr;
    inet_ntop(AF_INET, (char *)&sa_in->sin_addr, sk_addr, sizeof(sk_addr));
    snprintf(sk_addr + strlen(sk_addr),
	     sizeof(sk_addr) - strlen(sk_addr), ":%d", ntohs(sa_in->sin_port));
    size = size > SOCKADDRLEN - 1 ? SOCKADDRLEN - 1 : size;
    sock[size] = '\0';
    memcpy(sock, sk_addr, size);
    return 0;
}

int sk_peername(int sfd, char *peer, int size) {
    struct sockaddr_storage addr = {};
    socklen_t addrlen = sizeof(addr);
    struct sockaddr_in *sa_in;
    char sk_addr[SOCKADDRLEN] = {};

    if (-1 == getpeername(sfd, (struct sockaddr *)&addr, &addrlen))
	return -1;
    sa_in = (struct sockaddr_in *)&addr;
    inet_ntop(AF_INET, (char *)&sa_in->sin_addr, sk_addr, sizeof(sk_addr));
    snprintf(sk_addr + strlen(sk_addr),
	     sizeof(sk_addr) - strlen(sk_addr), ":%d", ntohs(sa_in->sin_port));
    size = size > SOCKADDRLEN - 1 ? SOCKADDRLEN - 1 : size;
    peer[size] = '\0';
    memcpy(peer, sk_addr, size);
    return 0;
}


int sk_reconnect(int *sfd) {
    int nfd;
    char sock[SOCKADDRLEN] = {}, peer[SOCKADDRLEN] = {};

    if (sk_sockname(*sfd, sock, SOCKADDRLEN) < 0 || sk_peername(*sfd, peer, SOCKADDRLEN) < 0)
	return -1;
    if ((nfd = __tcp_connect(sock, peer)) < 0)
	return -1;
    close(*sfd);
    *sfd = nfd;
    return 0;
}
