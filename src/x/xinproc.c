#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "xbase.h"

static int xinp_put(struct xsock *sx) {
    int old;
    mutex_lock(&sx->lock);
    old = sx->proc.ref--;
    sx->fok = false;
    mutex_unlock(&sx->lock);
    return old;
}

extern struct xsock *find_listener(char *addr);


/******************************************************************************
 *  snd_head events trigger.
 ******************************************************************************/

static int snd_head_push(int xd) {
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

static int rcv_head_pop(int xd) {
    int rc = 0;
    struct xsock *sx = xget(xd);

    if (sx->snd_waiters)
	condition_signal(&sx->cond);
    return rc;
}



/******************************************************************************
 *  xsock_inproc_protocol
 ******************************************************************************/

static int xinp_connector_init(int xd) {
    struct xsock *sx = xget(xd);
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

static void xinp_connector_destroy(int xd) {
    struct xsock *sx = xget(xd);    
    struct xsock *peer = sx->proc.peer_xsock;

    /* Destroy the xsock and free xsock id if i hold the last ref. */
    if (xinp_put(peer) == 1) {
	xsock_free(peer);
    }
    if (xinp_put(sx) == 1) {
	xsock_free(sx);
    }
}


static void snd_head_notify(int xd, uint32_t events) {
    if (events & XMQ_PUSH)
	snd_head_push(xd);
}

static void rcv_head_notify(int xd, uint32_t events) {
    if (events & XMQ_POP)
	rcv_head_pop(xd);
}

struct xsock_protocol xinp_connector_protocol = {
    .type = XCONNECTOR,
    .pf = PF_INPROC,
    .init = xinp_connector_init,
    .destroy = xinp_connector_destroy,
    .snd_notify = snd_head_notify,
    .rcv_notify = rcv_head_notify,
};
