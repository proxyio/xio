#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sync/waitgroup.h>
#include <runner/taskpool.h>
#include <transport/sockaddr.h>
#include "xgb.h"

int push_request_sock(struct xsock *sx, struct xsock *req_sx) {
    int rc = 0;

    while (sx->parent >= 0)
	sx = xget(sx->parent);

    mutex_lock(&sx->lock);
    if (list_empty(&sx->request_socks) && sx->accept_waiters > 0) {
	condition_broadcast(&sx->accept_cond);
    }
    list_add_tail(&req_sx->rqs_link, &sx->request_socks);
    __xpoll_notify(sx, XPOLLIN);
    mutex_unlock(&sx->lock);
    return rc;
}

struct xsock *pop_request_sock(struct xsock *sx) {
    struct xsock *req_sx = 0;

    mutex_lock(&sx->lock);
    while (list_empty(&sx->request_socks) && !sx->fasync) {
	sx->accept_waiters++;
	condition_wait(&sx->accept_cond, &sx->lock);
	sx->accept_waiters--;
    }
    if (!list_empty(&sx->request_socks)) {
	req_sx = list_first(&sx->request_socks, struct xsock, rqs_link);
	list_del_init(&req_sx->rqs_link);
    }
    mutex_unlock(&sx->lock);
    return req_sx;
}

int xaccept(int xd) {
    struct xsock *sx = xget(xd);
    struct xsock *new_sx = 0;

    if (!sx->fok) {
	errno = EPIPE;
	return -1;
    }
    if (sx->type != XLISTENER) {
	errno = EPROTO;
	return -1;
    }
    if ((new_sx = pop_request_sock(sx)))
	return new_sx->xd;
    errno = EAGAIN;
    return -1;
}

int _xlisten(int pf, const char *addr) {
    int xd = xsocket(pf, XLISTENER);

    if (pf <= 0) {
	errno = EPROTO;
	return -1;
    }
    if (xd < 0) {
	errno = EMFILE;
	return -1;
    }
    if (xbind(xd, addr) != 0)
	return -1;
    return xd;
}


int xlisten(const char *addr) {
    int pf = sockaddr_pf(addr);
    char sx_addr[TP_SOCKADDRLEN] = {};

    if (sockaddr_addr(addr, sx_addr, sizeof(sx_addr)) != 0)
	return -1;
    return _xlisten(pf, sx_addr);
}
