#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "xbase.h"

static int xinproc_put(struct xsock *sx) {
    int old;
    mutex_lock(&sx->lock);
    old = sx->proc.ref--;
    sx->fok = false;
    mutex_unlock(&sx->lock);
    return old;
}

/******************************************************************************
 *  xsock's proc field operation.
 ******************************************************************************/

static struct xsock *find_listener(char *addr) {
    struct ssmap_node *node;
    struct xsock *sx = 0;

    xglobal_lock();
    if ((node = ssmap_find(&xgb.inproc_listeners, addr, TP_SOCKADDRLEN)))
	sx = cont_of(node, struct xsock, proc.rb_link);
    xglobal_unlock();
    return sx;
}

static int insert_listener(struct ssmap_node *node) {
    int rc = -1;

    errno = EADDRINUSE;
    xglobal_lock();
    if (!ssmap_find(&xgb.inproc_listeners, node->key, node->keylen)) {
	rc = 0;
	ssmap_insert(&xgb.inproc_listeners, node);
    }
    xglobal_unlock();
    return rc;
}


static void remove_listener(struct ssmap_node *node) {
    xglobal_lock();
    ssmap_delete(&xgb.inproc_listeners, node);
    xglobal_unlock();
}


static void push_new_connector(struct xsock *sx, struct xsock *new) {
    mutex_lock(&sx->lock);
    list_add_tail(&new->proc.at_link, &sx->proc.at_queue);
    mutex_unlock(&sx->lock);
}

static struct xsock *pop_new_connector(struct xsock *sx) {
    struct xsock *new = 0;

    mutex_lock(&sx->lock);
    if (!list_empty(&sx->proc.at_queue)) {
	new = list_first(&sx->proc.at_queue, struct xsock, proc.at_link);
	list_del_init(&new->proc.at_link);
    }
    mutex_unlock(&sx->lock);
    return new;
}

/******************************************************************************
 *  snd_head events trigger.
 ******************************************************************************/

static int snd_push_event(int xd) {
    int rc = 0, can = false;
    struct xmsg *msg;
    struct xsock *sx = xget(xd);
    struct xsock *peer = sx->proc.peer_xsock;

    // Unlock myself first because i hold the lock
    mutex_unlock(&sx->lock);

    // TODO: maybe the peer xsock can't recv anymore after the check.
    mutex_lock(&peer->lock);
    if (can_recv(peer))
	can = true;
    mutex_unlock(&peer->lock);
    if (!can)
	return -1;
    if ((msg = pop_snd(sx)))
	push_rcv(peer, msg);

    mutex_lock(&sx->lock);
    return rc;
}

/******************************************************************************
 *  rcv_head events trigger.
 ******************************************************************************/

static int rcv_pop_event(int xd) {
    int rc = 0;
    struct xsock *sx = xget(xd);

    if (sx->snd_waiters)
	condition_signal(&sx->cond);
    return rc;
}



/******************************************************************************
 *  xsock_inproc_protocol
 ******************************************************************************/

static int inproc_listener_init(int xd) {
    int rc = 0;
    struct xsock *sx = xget(xd);
    struct ssmap_node *node = &sx->proc.rb_link;

    node->key = sx->addr;
    node->keylen = TP_SOCKADDRLEN;
    if ((rc = insert_listener(node)) < 0)
	return rc;
    return rc;
}


static int inproc_listener_destroy(int xd) {
    int rc = 0;
    struct xsock *sx = xget(xd);
    struct xsock *new;

    /* Avoiding the new connectors */
    remove_listener(&sx->proc.rb_link);

    while ((new = pop_new_connector(sx))) {
	mutex_lock(&new->lock);
	/* Sending a ECONNREFUSED signel to the other peer. */
	if (new->proc.ref-- == 1)
	    condition_signal(&new->cond);
	mutex_unlock(&new->lock);
    }

    /* Destroy the xsock and free xsock id. */
    xsock_free(sx);
    return rc;
}


static int inproc_connector_init(int xd) {
    struct xsock *sx = xget(xd);
    /* TODO: reference safe */
    struct xsock *req_sx = xsock_alloc();
    struct xsock *listener = find_listener(sx->peer);

    if (!req_sx) {
	errno = EAGAIN;
	return -1;
    }
    if (!listener) {
	errno = ENOENT;	
	return -1;
    }
    ZERO(req_sx->proc);
    req_sx->ty = XCONNECTOR;
    req_sx->pf = sx->pf;
    req_sx->l4proto = sx->l4proto;
    
    req_sx->proc.ref = sx->proc.ref = 2;
    sx->proc.peer_xsock = req_sx;
    req_sx->proc.peer_xsock = sx;
    req_sx->pf = sx->pf;
    req_sx->l4proto = sx->l4proto;
    push_request_sock(listener, req_sx);
    return 0;
}

static int inproc_connector_destroy(int xd) {
    int rc = 0;
    struct xsock *sx = xget(xd);    
    struct xsock *peer = sx->proc.peer_xsock;

    /* Destroy the xsock and free xsock id if i hold the last ref. */
    if (xinproc_put(peer) == 1) {
	xsock_free(peer);
    }
    if (xinproc_put(sx) == 1) {
	xsock_free(sx);
    }
    return rc;
}


static int inproc_xinit(int xd) {
    struct xsock *sx = xget(xd);

    ZERO(sx->proc);
    INIT_LIST_HEAD(&sx->proc.at_queue);

    switch (sx->ty) {
    case XCONNECTOR:
	return inproc_connector_init(xd);
    case XLISTENER:
	return inproc_listener_init(xd);
    }
    errno = EINVAL;
    return -1;
}

static void inproc_xdestroy(int xd) {
    struct xsock *sx = xget(xd);

    switch (sx->ty) {
    case XCONNECTOR:
	inproc_connector_destroy(xd);
	break;
    case XLISTENER:
	inproc_listener_destroy(xd);
	break;
    default:
	BUG_ON(1);
    }
}

static void inproc_snd_notify(int xd, uint32_t events) {
    if (events & XMQ_PUSH)
	snd_push_event(xd);
}

static void inproc_rcv_notify(int xd, uint32_t events) {
    if (events & XMQ_POP)
	rcv_pop_event(xd);
}

struct xsock_protocol xinproc_connector_protocol = {
    .type = XCONNECTOR,
    .pf = PF_INPROC,
    .init = inproc_xinit,
    .destroy = inproc_xdestroy,
    .snd_notify = inproc_snd_notify,
    .rcv_notify = inproc_rcv_notify,
};
