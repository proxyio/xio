#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "xbase.h"

static int xinproc_put(struct xsock *cn) {
    int old;
    mutex_lock(&cn->lock);
    old = cn->proc.ref--;
    cn->fok = false;
    mutex_unlock(&cn->lock);
    return old;
}

/******************************************************************************
 *  channel's proc field operation.
 ******************************************************************************/

static struct xsock *find_listener(char *addr) {
    struct ssmap_node *node;
    struct xsock *cn = NULL;

    xglobal_lock();
    if ((node = ssmap_find(&xgb.inproc_listeners, addr, TP_SOCKADDRLEN)))
	cn = cont_of(node, struct xsock, proc.listener_node);
    xglobal_unlock();
    return cn;
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


static void push_new_connector(struct xsock *cn, struct xsock *new) {
    mutex_lock(&cn->lock);
    list_add_tail(&new->proc.wait_item, &cn->proc.new_connectors);
    mutex_unlock(&cn->lock);
}

static struct xsock *pop_new_connector(struct xsock *cn) {
    struct xsock *new = NULL;

    mutex_lock(&cn->lock);
    if (!list_empty(&cn->proc.new_connectors)) {
	new = list_first(&cn->proc.new_connectors, struct xsock, proc.wait_item);
	list_del_init(&new->proc.wait_item);
    }
    mutex_unlock(&cn->lock);
    return new;
}

/******************************************************************************
 *  snd_head events trigger.
 ******************************************************************************/

static int snd_push_event(int cd) {
    int rc = 0, can = false;
    struct xmsg *msg;
    struct xsock *cn = xget(cd);
    struct xsock *peer = cn->proc.peer_channel;

    // Unlock myself first because i hold the lock
    mutex_unlock(&cn->lock);

    // TODO: maybe the peer channel can't recv anymore after the check.
    mutex_lock(&peer->lock);
    if (can_recv(peer))
	can = true;
    mutex_unlock(&peer->lock);
    if (!can)
	return -1;
    if ((msg = pop_snd(cn)))
	push_rcv(peer, msg);

    mutex_lock(&cn->lock);
    return rc;
}

/******************************************************************************
 *  rcv_head events trigger.
 ******************************************************************************/

static int rcv_pop_event(int cd) {
    int rc = 0;
    struct xsock *cn = xget(cd);

    if (cn->snd_waiters)
	condition_signal(&cn->cond);
    return rc;
}




/******************************************************************************
 *  xsock_vfptr
 ******************************************************************************/

static int inproc_accepter_init(int cd) {
    int rc = 0;
    struct xsock *me = xget(cd);
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

static int inproc_listener_init(int cd) {
    int rc = 0;
    struct xsock *cn = xget(cd);
    struct ssmap_node *node = &cn->proc.listener_node;

    node->key = cn->addr;
    node->keylen = TP_SOCKADDRLEN;
    INIT_LIST_HEAD(&cn->proc.new_connectors);
    if ((rc = insert_listener(node)) < 0)
	return rc;
    return rc;
}


static int inproc_listener_destroy(int cd) {
    int rc = 0;
    struct xsock *cn = xget(cd);
    struct xsock *new;

    /* Avoiding the new connectors */
    remove_listener(&cn->proc.listener_node);

    while ((new = pop_new_connector(cn))) {
	mutex_lock(&new->lock);
	/* Sending a ECONNREFUSED signel to the other peer. */
	if (new->proc.ref-- == 1)
	    condition_signal(&new->cond);
	mutex_unlock(&new->lock);
    }

    /* Destroy the channel and free channel id. */
    xsock_free(cn);
    return rc;
}


static int inproc_connector_init(int cd) {
    int rc = 0;
    struct xsock *cn = xget(cd);
    struct xsock *listener = find_listener(cn->peer);

    if (!listener) {
	errno = ENOENT;	
	return -1;
    }
    cn->proc.ref = 0;
    cn->proc.peer_channel = NULL;

    /* step1. Push the new connector into listener's new_connectors
     * queue and update_xpoll_t for user-state poll
     */
    push_new_connector(listener, cn);
    xpoll_notify(listener, XPOLLIN);

    /* step2. Hold lock and waiting for the connection established
     * if need. here only has two possible state too:
     * if cn->proc.ref == 0. we incr the ref indicate that i'm waiting.
     */
    mutex_lock(&cn->lock);
    if (cn->proc.ref == 0) {
	cn->proc.ref++;
	condition_wait(&cn->cond, &cn->lock);
    }

    /* step3. Check the connection status.
     * Maybe the other peer close the connection before the ESTABLISHED
     * if cn->proc.ref == 0. the peer was closed.
     * if cn->proc.ref == 2. the connect was established.
     */
    if (cn->proc.ref == -1)
	BUG_ON(cn->proc.ref != -1);
    else if (cn->proc.ref == 0)
	BUG_ON(cn->proc.ref != 0);
    else if (cn->proc.ref == 2)
	BUG_ON(cn->proc.ref != 2);
    if (cn->proc.ref == 0 || cn->proc.ref == -1) {
    	errno = ECONNREFUSED;
	rc = -1;
    }
    mutex_unlock(&cn->lock);
    return rc;
}

static int inproc_connector_destroy(int cd) {
    int rc = 0;
    struct xsock *cn = xget(cd);    
    struct xsock *peer = cn->proc.peer_channel;

    /* Destroy the channel and free channel id if i hold the last ref. */
    if (xinproc_put(peer) == 1) {
	xsock_free(peer);
    }
    if (xinproc_put(cn) == 1) {
	xsock_free(cn);
    }
    return rc;
}


static int inproc_xinit(int cd) {
    struct xsock *cn = xget(cd);

    switch (cn->ty) {
    case XACCEPTER:
	return inproc_accepter_init(cd);
    case XCONNECTOR:
	return inproc_connector_init(cd);
    case XLISTENER:
	return inproc_listener_init(cd);
    }
    return -EINVAL;
}

static void inproc_xdestroy(int cd) {
    struct xsock *cn = xget(cd);

    switch (cn->ty) {
    case XACCEPTER:
    case XCONNECTOR:
	inproc_connector_destroy(cd);
	break;
    case XLISTENER:
	inproc_listener_destroy(cd);
	break;
    default:
	BUG_ON(1);
    }
}

static void inproc_snd_notify(int cd, uint32_t events) {
    if (events & XMQ_PUSH)
	snd_push_event(cd);
}

static void inproc_rcv_notify(int cd, uint32_t events) {
    if (events & XMQ_POP)
	rcv_pop_event(cd);
}

static struct xsock_vf inproc_xsock_vf = {
    .pf = PF_INPROC,
    .init = inproc_xinit,
    .destroy = inproc_xdestroy,
    .snd_notify = inproc_snd_notify,
    .rcv_notify = inproc_rcv_notify,
};

struct xsock_vf *inproc_xsock_vfptr = &inproc_xsock_vf;
