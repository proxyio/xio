#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "channel_base.h"

extern struct channel_global cn_global;

extern struct channel *cid_to_channel(int cd);
extern void free_channel(struct channel *cn);

extern struct channel_msg *pop_rcv(struct channel *cn);
extern void push_rcv(struct channel *cn, struct channel_msg *msg);
extern struct channel_msg *pop_snd(struct channel *cn);
extern int push_snd(struct channel *cn, struct channel_msg *msg);


static int channel_put(struct channel *cn) {
    int old;
    mutex_lock(&cn->lock);
    old = cn->proc.ref--;
    cn->fok = false;
    mutex_unlock(&cn->lock);
    return old;
}

static struct channel *find_listener(char *addr) {
    struct ssmap_node *node;
    struct channel *cn = NULL;

    cn_global_lock();
    if ((node = ssmap_find(&cn_global.inproc_listeners, addr, TP_SOCKADDRLEN)))
	cn = cont_of(node, struct channel, proc.listener_node);
    cn_global_unlock();
    return cn;
}

static int insert_listener(struct ssmap_node *node) {
    int rc = -1;

    errno = EADDRINUSE;
    cn_global_lock();
    if (!ssmap_find(&cn_global.inproc_listeners, node->key, node->keylen)) {
	rc = 0;
	ssmap_insert(&cn_global.inproc_listeners, node);
    }
    cn_global_unlock();
    return rc;
}


static void remove_listener(struct ssmap_node *node) {
    cn_global_lock();
    ssmap_delete(&cn_global.inproc_listeners, node);
    cn_global_unlock();
}


static void push_new_connector(struct channel *cn, struct channel *new) {
    mutex_lock(&cn->lock);
    list_add_tail(&new->proc.wait_item, &cn->proc.new_connectors);
    mutex_unlock(&cn->lock);
}

static struct channel *pop_new_connector(struct channel *cn) {
    struct channel *new = NULL;

    mutex_lock(&cn->lock);
    if (!list_empty(&cn->proc.new_connectors)) {
	new = list_first(&cn->proc.new_connectors, struct channel, proc.wait_item);
	list_del_init(&new->proc.wait_item);
    }
    mutex_unlock(&cn->lock);
    return new;
}

/******************************************************************************
 *  snd_head events trigger.
 ******************************************************************************/

static int snd_head_push(int cd) {
    int rc = 0, can = false;
    struct channel_msg *msg;
    struct channel *cn = cid_to_channel(cd);
    struct channel *peer = cn->proc.peer_channel;

    // TODO: maybe the peer channel can't recv anymore after the check.
    mutex_lock(&peer->lock);
    if (can_recv(peer))
	can = true;
    mutex_unlock(&peer->lock);
    if (!can)
	return -1;
    if ((msg = pop_snd(cn)))
	push_rcv(peer, msg);
    return rc;
}

static int snd_head_pop(int cd) {
    int rc = 0;
    return rc;
}

static int snd_head_empty(int cd) {
    int rc = 0;
    return rc;
}

static int snd_head_nonempty(int cd) {
    int rc = 0;
    return rc;
}

static int snd_head_full(int cd) {
    int rc = 0;
    return rc;
}

static int snd_head_nonfull(int cd) {
    int rc = 0;
    return rc;
}

#define snd_head_vf {				\
	.push = snd_head_push,			\
	.pop = snd_head_pop,			\
	.empty = snd_head_empty,		\
	.nonempty = snd_head_nonempty,		\
	.full = snd_head_full,			\
	.nonfull = snd_head_nonfull,		\
    }

/******************************************************************************
 *  rcv_head events trigger.
 ******************************************************************************/

static int rcv_head_push(int cd) {
    int rc = 0;
    return rc;
}

static int rcv_head_pop(int cd) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    mutex_lock(&cn->lock);
    if (cn->snd_waiters)
	condition_signal(&cn->cond);
    mutex_unlock(&cn->lock);
    return rc;
}

static int rcv_head_empty(int cd) {
    int rc = 0;
    return rc;
}

static int rcv_head_nonempty(int cd) {
    int rc = 0;
    return rc;
}

static int rcv_head_full(int cd) {
    int rc = 0;
    return rc;
}

static int rcv_head_nonfull(int cd) {
    int rc = 0;
    return rc;
}

#define rcv_head_vf {				\
	.push = rcv_head_push,			\
	.pop = rcv_head_pop,			\
	.empty = rcv_head_empty,		\
	.nonempty = rcv_head_nonempty,		\
	.full = rcv_head_full,			\
	.nonfull = rcv_head_nonfull,		\
    }


static int inproc_accepter_init(int cd) {
    int rc = 0;
    struct channel *me = cid_to_channel(cd);
    struct channel *peer;
    struct channel *parent = cid_to_channel(me->parent);

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
    assert(peer->proc.ref == 0 || peer->proc.ref == 1);
    if (peer->proc.ref == 1)
	condition_signal(&peer->cond);
    me->proc.ref = peer->proc.ref = 2;
    mutex_unlock(&peer->lock);

    return rc;
}

static int inproc_listener_init(int cd) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);
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
    struct channel *cn = cid_to_channel(cd);
    struct channel *new;

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
    free_channel(cn);
    return rc;
}


static int inproc_connector_init(int cd) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);
    struct channel *listener = find_listener(cn->peer);

    errno = ENOENT;
    if (!listener)
	return -1;
    cn->proc.ref = 0;
    cn->proc.peer_channel = NULL;

    /* step1. Push the new connector into listener's new_connectors
       queue */
    push_new_connector(listener, cn);

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
	assert(cn->proc.ref == -1);
    else if (cn->proc.ref == 0)
	assert(cn->proc.ref == 0);
    else if (cn->proc.ref == 2)
	assert(cn->proc.ref == 2);
    if (cn->proc.ref == 0 || cn->proc.ref == -1) {
    	errno = ECONNREFUSED;
	rc = -1;
    }
    mutex_unlock(&cn->lock);
    return rc;
}

static int inproc_connector_destroy(int cd) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);    
    struct channel *peer = cn->proc.peer_channel;

    /* Destroy the channel and free channel id if i hold the last ref. */
    if (channel_put(peer) == 1)
	free_channel(peer);
    if (channel_put(cn) == 1)
	free_channel(cn);
    return rc;
}


static int inproc_channel_init(int cd) {
    struct channel *cn = cid_to_channel(cd);

    switch (cn->ty) {
    case CHANNEL_ACCEPTER:
	return inproc_accepter_init(cd);
    case CHANNEL_CONNECTOR:
	return inproc_connector_init(cd);
    case CHANNEL_LISTENER:
	return inproc_listener_init(cd);
    }
    return -EINVAL;
}

static void inproc_channel_destroy(int cd) {
    struct channel *cn = cid_to_channel(cd);

    switch (cn->ty) {
    case CHANNEL_ACCEPTER:
    case CHANNEL_CONNECTOR:
	inproc_connector_destroy(cd);
	break;
    case CHANNEL_LISTENER:
	inproc_listener_destroy(cd);
	break;
    default:
	assert(0);
    }
}

static int inproc_channel_setopt(int cd, int opt, void *val, int valsz) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    mutex_lock(&cn->lock);
    mutex_unlock(&cn->lock);
    return rc;
}

static int inproc_channel_getopt(int cd, int opt, void *val, int valsz) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    mutex_lock(&cn->lock);
    mutex_unlock(&cn->lock);
    return rc;
}

static struct channel_vf inproc_channel_vf = {
    .pf = PF_INPROC,
    .init = inproc_channel_init,
    .destroy = inproc_channel_destroy,
    .setopt = inproc_channel_setopt,
    .getopt = inproc_channel_getopt,
    .rcv_notify = rcv_head_vf,
    .snd_notify = snd_head_vf,
};

struct channel_vf *inproc_channel_vfptr = &inproc_channel_vf;
