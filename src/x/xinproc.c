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
 *  channel's proc field operation.
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
    struct xsock *peer = sx->proc.peer_channel;

    // Unlock myself first because i hold the lock
    mutex_unlock(&sx->lock);

    // TODO: maybe the peer channel can't recv anymore after the check.
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

static int inproc_accepter_init(int xd) {
    int rc = 0;
    struct xsock *me = xget(xd);
    struct xsock *peer;
    struct xsock *parent = xget(me->parent);

    /* step1. Pop a new connector from parent's channel queue */
    if (!(peer = pop_new_connector(parent)))
	return -1;

    /* step2. Hold the peer's lock and make a connection. */
    mutex_lock(&peer->lock);

    /* Each channel endpoint has one ref to another endpoint */
    peer->proc.peer_channel = me;
    me->proc.peer_channel = peer;

    /* Send the ACK singal to the other end.
     * Here only has two possible state:
     * 1. if peer->proc.ref == 0. the peer haven't enter step2 state.
     * 2. if peer->proc.ref == 1. the peer is waiting and we should
     *    wakeup him when we done the connect work.
     */
    BUG_ON(peer->proc.ref != 0 && peer->proc.ref != 1);
    if (peer->proc.ref == 1)
	condition_signal(&peer->cond);
    me->proc.ref = peer->proc.ref = 2;
    mutex_unlock(&peer->lock);

    return rc;
}

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

    /* Destroy the channel and free channel id. */
    xsock_free(sx);
    return rc;
}


static int inproc_connector_init(int xd) {
    int rc = 0;
    struct xsock *sx = xget(xd);
    struct xsock *listener = find_listener(sx->peer);

    if (!listener) {
	errno = ENOENT;	
	return -1;
    }
    sx->proc.ref = 0;
    sx->proc.peer_channel = 0;

    /* step1. Push the new connector into listener's at_queue
     * queue and update_xpoll_t for user-state poll
     */
    push_new_connector(listener, sx);
    xpoll_notify(listener, XPOLLIN);

    /* step2. Hold lock and waiting for the connection established
     * if need. here only has two possible state too:
     * if sx->proc.ref == 0. we incr the ref indicate that i'm waiting.
     */
    mutex_lock(&sx->lock);
    if (sx->proc.ref == 0) {
	sx->proc.ref++;
	condition_wait(&sx->cond, &sx->lock);
    }

    /* step3. Check the connection status.
     * Maybe the other peer close the connection before the ESTABLISHED
     * if sx->proc.ref == 0. the peer was closed.
     * if sx->proc.ref == 2. the connect was established.
     */
    if (sx->proc.ref == -1)
	BUG_ON(sx->proc.ref != -1);
    else if (sx->proc.ref == 0)
	BUG_ON(sx->proc.ref != 0);
    else if (sx->proc.ref == 2)
	BUG_ON(sx->proc.ref != 2);
    if (sx->proc.ref == 0 || sx->proc.ref == -1) {
    	errno = ECONNREFUSED;
	rc = -1;
    }
    mutex_unlock(&sx->lock);
    return rc;
}

static int inproc_connector_destroy(int xd) {
    int rc = 0;
    struct xsock *sx = xget(xd);    
    struct xsock *peer = sx->proc.peer_channel;

    /* Destroy the channel and free channel id if i hold the last ref. */
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
    case XACCEPTER:
	return inproc_accepter_init(xd);
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
    case XACCEPTER:
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

struct xsock_protocol inproc_xsock_protocol = {
    .pf = PF_INPROC,
    .init = inproc_xinit,
    .destroy = inproc_xdestroy,
    .snd_notify = inproc_snd_notify,
    .rcv_notify = inproc_rcv_notify,
};
