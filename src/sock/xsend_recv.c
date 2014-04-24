#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sync/waitgroup.h>
#include "runner/taskpool.h"
#include "xbase.h"

struct xmsg *pop_rcv(struct xsock *sx) {
    struct xmsg *msg = 0;
    struct xsock_protocol *l4proto = sx->l4proto;
    i64 msgsz;
    u32 events = 0;

    mutex_lock(&sx->lock);
    while (list_empty(&sx->rcv_head) && !sx->fasync) {
	sx->rcv_waiters++;
	condition_wait(&sx->cond, &sx->lock);
	sx->rcv_waiters--;
    }
    if (!list_empty(&sx->rcv_head)) {
	DEBUG_OFF("xsock %d", sx->xd);
	msg = list_first(&sx->rcv_head, struct xmsg, item);
	list_del_init(&msg->item);
	msgsz = xiov_len(msg->vec.chunk);
	sx->rcv -= msgsz;
	events |= XMQ_POP;
	if (sx->rcv_wnd - sx->rcv <= msgsz)
	    events |= XMQ_NONFULL;
	if (list_empty(&sx->rcv_head)) {
	    BUG_ON(sx->rcv);
	    events |= XMQ_EMPTY;
	}
    }

    if (events && l4proto->rcv_notify)
	l4proto->rcv_notify(sx->xd, events);

    mutex_unlock(&sx->lock);
    return msg;
}

void push_rcv(struct xsock *sx, struct xmsg *msg) {
    struct xsock_protocol *l4proto = sx->l4proto;
    u32 events = 0;
    i64 msgsz = xiov_len(msg->vec.chunk);

    mutex_lock(&sx->lock);
    if (list_empty(&sx->rcv_head))
	events |= XMQ_NONEMPTY;
    if (sx->rcv_wnd - sx->rcv <= msgsz)
	events |= XMQ_FULL;
    events |= XMQ_PUSH;
    sx->rcv += msgsz;
    list_add_tail(&msg->item, &sx->rcv_head);    
    __xpoll_notify(sx, 0);
    DEBUG_OFF("xsock %d", sx->xd);

    /* Wakeup the blocking waiters. */
    if (sx->rcv_waiters > 0)
	condition_broadcast(&sx->cond);

    if (events && l4proto->rcv_notify)
	l4proto->rcv_notify(sx->xd, events);
    mutex_unlock(&sx->lock);
}


struct xmsg *pop_snd(struct xsock *sx) {
    struct xsock_protocol *l4proto = sx->l4proto;
    struct xmsg *msg = 0;
    i64 msgsz;
    u32 events = 0;
    
    mutex_lock(&sx->lock);
    if (!list_empty(&sx->snd_head)) {
	DEBUG_OFF("xsock %d", sx->xd);
	msg = list_first(&sx->snd_head, struct xmsg, item);
	list_del_init(&msg->item);
	msgsz = xiov_len(msg->vec.chunk);
	sx->snd -= msgsz;
	events |= XMQ_POP;
	if (sx->snd_wnd - sx->snd <= msgsz)
	    events |= XMQ_NONFULL;
	if (list_empty(&sx->snd_head)) {
	    BUG_ON(sx->snd);
	    events |= XMQ_EMPTY;
	}

	/* Wakeup the blocking waiters */
	if (sx->snd_waiters > 0)
	    condition_broadcast(&sx->cond);
    }

    if (events && l4proto->snd_notify)
	l4proto->snd_notify(sx->xd, events);

    __xpoll_notify(sx, 0);
    mutex_unlock(&sx->lock);
    return msg;
}

int push_snd(struct xsock *sx, struct xmsg *msg) {
    int rc = -1;
    struct xsock_protocol *l4proto = sx->l4proto;
    u32 events = 0;
    i64 msgsz = xiov_len(msg->vec.chunk);

    mutex_lock(&sx->lock);
    while (!can_send(sx) && !sx->fasync) {
	sx->snd_waiters++;
	condition_wait(&sx->cond, &sx->lock);
	sx->snd_waiters--;
    }
    if (can_send(sx)) {
	rc = 0;
	if (list_empty(&sx->snd_head))
	    events |= XMQ_NONEMPTY;
	if (sx->snd_wnd - sx->snd <= msgsz)
	    events |= XMQ_FULL;
	events |= XMQ_PUSH;
	sx->snd += msgsz;
	list_add_tail(&msg->item, &sx->snd_head);
	DEBUG_OFF("xsock %d", sx->xd);
    }

    if (events && l4proto->snd_notify)
	l4proto->snd_notify(sx->xd, events);

    mutex_unlock(&sx->lock);
    return rc;
}

int xrecv(int xd, char **xbuf) {
    int rc = 0;
    struct xmsg *msg = 0;
    struct xsock *sx = xget(xd);
    
    if (!xbuf) {
	errno = EINVAL;
	return -1;
    }
    if (sx->type != XCONNECTOR) {
	errno = EPROTO;
	return -1;
    }
    if (!(msg = pop_rcv(sx))) {
	errno = sx->fok ? EAGAIN : EPIPE;
	rc = -1;
    } else
	*xbuf = msg->vec.chunk;
    return rc;
}

int xsend(int xd, char *xbuf) {
    int rc = 0;
    struct xmsg *msg = 0;
    struct xsock *sx = xget(xd);

    if (!xbuf) {
	errno = EINVAL;
	return -1;
    }
    if (sx->type != XCONNECTOR) {
	errno = EPROTO;
	return -1;
    }
    msg = cont_of(xbuf, struct xmsg, vec.chunk);
    if ((rc = push_snd(sx, msg)) < 0) {
	errno = sx->fok ? EAGAIN : EPIPE;
    }
    return rc;
}
