#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sync/waitgroup.h>
#include "runner/taskpool.h"
#include "xbase.h"

static void xshutdown(struct xsock *sx) {
    struct xcpu *cpu = xcpuget(sx->cpu_no);
    struct xtask *ts = &sx->shutdown;    

    mutex_lock(&cpu->lock);
    while (efd_signal(&cpu->efd) < 0) {
	/* Pipe is full and another thread is unsignaling. we release */
	mutex_unlock(&cpu->lock);
	mutex_lock(&cpu->lock);
    }
    if (!sx->fclosed && !attached(&ts->link)) {
	sx->fclosed = true;
	list_add_tail(&ts->link, &cpu->shutdown_socks);
    }
    mutex_unlock(&cpu->lock);
}

static void xmultiple_close(int xd);

int xshutdown_task_f(struct xtask *ts) {
    struct xsock *sx = cont_of(ts, struct xsock, shutdown);

    DEBUG_OFF("xsock %d shutdown protocol %s", sx->xd, xprotocol_str[sx->pf]);
    if (sx->l4proto)
	sx->l4proto->close(sx->xd);
    else if (!list_empty(&sx->sub_socks)) {
	xmultiple_close(sx->xd);
	xsock_free(sx);
	DEBUG_OFF("xsock %d multiple_close", sx->xd);
    }
    return 0;
}

void xclose(int xd) {
    struct xsock *sx = xget(xd);
    struct xpoll_t *po;
    struct xpoll_entry *ent, *nx;
    struct list_head xpoll_head = {};

    INIT_LIST_HEAD(&xpoll_head);
    mutex_lock(&sx->lock);
    list_splice(&sx->xpoll_head, &xpoll_head);
    mutex_unlock(&sx->lock);

    xsock_walk_ent(ent, nx, &xpoll_head) {
	po = cont_of(ent->notify, struct xpoll_t, notify);
	xpoll_ctl(po, XPOLL_DEL, &ent->event);
	__detach_from_xsock(ent);
	xent_put(ent);
    }

    /* Let backend thread do the last destroy(). */
    xshutdown(sx);
}


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

int xsocket(int pf, int type) {
    struct xsock *sx = xsock_alloc();

    if (!sx) {
	errno = EMFILE;
	return -1;
    }
    if (!(sx->l4proto = l4proto_lookup(pf, type))
	&& (type != XLISTENER || (pf & ~PF_MULE) || !(pf & PF_MULE))) {
	errno = EPROTO;
	return -1;
    }
    sx->pf = pf;
    sx->type = type;
    return sx->xd;
}

static void xmultiple_close(int xd) {
    struct xsock *sub_sx, *nx;
    struct xsock *sx = xget(xd);

    xsock_walk_sub_socks(sub_sx, nx, &sx->sub_socks) {
	sub_sx->parent = -1;
	list_del_init(&sub_sx->sib_link);
	xclose(sub_sx->xd);
    }
}

static int xmultiple_listen(int xd, const char *addr) {
    int sub_xd;
    struct xsock_protocol *l4proto, *nx;
    struct xsock *sx = xget(xd), *sub_sx;
    int pf = sx->pf;

    xsock_protocol_walk_safe(l4proto, nx, &xgb.xsock_protocol_head) {
	if (!(pf & l4proto->pf) || l4proto->type != XLISTENER)
	    continue;
	pf &= ~l4proto->pf;
	if ((sub_xd = xlisten(l4proto->pf, addr)) < 0)
	    goto BAD;
	sub_sx = xget(sub_xd);
	sub_sx->parent = xd;
	list_add_tail(&sub_sx->sib_link, &sx->sub_socks);
    }
    if (!list_empty(&sx->sub_socks))
	return 0;
 BAD:
    xmultiple_close(xd);
    return -1;
}

int xbind(int xd, const char *addr) {
    int rc;
    struct xsock *sx = xget(xd);

    if (sx->l4proto)
	rc = sx->l4proto->bind(xd, addr);
    else if (sx->type == XLISTENER)
	rc = xmultiple_listen(xd, addr);
    return rc;
}

int xsetopt(int xd, int opt, void *on, int size) {
    int rc = 0;
    struct xsock *sx = xget(xd);

    if (!on || size <= 0) {
	errno = EINVAL;
	return -1;
    }
    switch (opt) {
    case XNOBLOCK:
	mutex_lock(&sx->lock);
	sx->fasync = *(int *)on ? true : false;
	mutex_unlock(&sx->lock);
	break;
    case XSNDBUF:
	mutex_lock(&sx->lock);
	sx->snd_wnd = (*(int *)on);
	mutex_unlock(&sx->lock);
	break;
    case XRCVBUF:
	mutex_lock(&sx->lock);
	sx->rcv_wnd = (*(int *)on);
	mutex_unlock(&sx->lock);
	break;
    default:
	errno = EINVAL;
	return -1;
    }
    return rc;
}

int xgetopt(int xd, int opt, void *on, int size) {
    int rc = 0;
    struct xsock *sx = xget(xd);

    if (!on || size <= 0) {
	errno = EINVAL;
	return -1;
    }
    switch (opt) {
    case XNOBLOCK:
	mutex_lock(&sx->lock);
	*(int *)on = sx->fasync ? true : false;
	mutex_unlock(&sx->lock);
	break;
    case XSNDBUF:
	mutex_lock(&sx->lock);
	*(int *)on = sx->snd_wnd;
	mutex_unlock(&sx->lock);
	break;
    case XRCVBUF:
	mutex_lock(&sx->lock);
	*(int *)on = sx->rcv_wnd;
	mutex_unlock(&sx->lock);
	break;
    default:
	errno = EINVAL;
	return -1;
    }
    return rc;
}
