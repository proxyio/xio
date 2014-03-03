#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "base.h"
#include "accepter.h"

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

static int __act_listen(const char *sock, int backlog) {
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


int act_listen(const char *net, const char *sock, int backlog) {
    if (STREQ(net, "") || (!STREQ(net, "tcp") && !STREQ(net, "tcp4") && !STREQ(net, "tcp6"))) {
	errno = EINVAL;
	return -1;
    }
    return __act_listen(sock, backlog);
}

int act_accept(int afd) {
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

static inline int act_set_block(int afd, int block) {
    int flags = 0;

    flags = fcntl(afd, F_GETFL, 0);
    if (flags == -1)
	flags = 0;
    if (!block)
	flags |= O_NONBLOCK;
    else
	flags &= ~O_NONBLOCK;
    return fcntl(afd, F_SETFL, flags);
}

static inline int act_set_reuseaddr(int afd, int reuse) {
    reuse = reuse == true ? 1 : 0;
    return setsockopt(afd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
}

int act_setopt(int afd, int optname, ...) {
    switch (optname) {
    case ACT_BLOCK:
	return act_set_block(afd, optname);
    case ACT_REUSEADDR:
	return act_set_reuseaddr(afd, optname);
    }
    return 0;
}
