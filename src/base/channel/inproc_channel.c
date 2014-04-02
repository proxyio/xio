#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "channel_base.h"

extern struct channel_global cn_global;

extern struct channel *cid_to_channel(int cd);
extern int alloc_cid();
extern void free_channel(struct channel *cn);

static int channel_get(struct channel *cn) {
    int old;
    mutex_lock(&cn->lock);
    old = cn->ref++;
    mutex_unlock(&cn->lock);
    return old;
}

static int channel_put(struct channel *cn) {
    int old;
    mutex_lock(&cn->lock);
    old = cn->ref--;
    mutex_unlock(&cn->lock);
    return old;
}

static struct channel *find_listener(char *sock) {
    struct ssmap_node *node;
    struct channel *cn = NULL;

    cn_global_lock();
    if ((node = ssmap_find(&cn_global.inproc_listeners, sock, strlen(sock))))
	cn = cont_of(node, struct channel, listener_node);
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
    list_add_tail(&new->wait_item, &cn->new_connectors);
    mutex_unlock(&cn->lock);
}

static struct channel *pop_new_connector(struct channel *cn) {
    struct channel *new = NULL;

    mutex_lock(&cn->lock);
    if (!list_empty(&cn->new_connectors)) {
	new = list_first(&cn->new_connectors, struct channel, wait_item);
	list_del_init(&new->wait_item);
    }
    mutex_unlock(&cn->lock);
    return new;
}





static int inproc_accepter_init(int cd) {
    int rc = 0;
    struct channel *me = cid_to_channel(cd);
    struct channel *peer;
    struct channel *parent = cid_to_channel(me->parent);

    if (!(peer = pop_new_connector(parent)))
	return -1;
    mutex_lock(&peer->lock);

    /* TODO: Set the peer_channel */
    peer->peer_channel = me;
    me->peer_channel = peer;

    /* Each channel endpoint has one ref to another endpoint */
    me->ref = peer->ref = 2;

    /* Send the DONE singal to the other end. */
    if (--peer->waiters == 0)
	condition_signal(&peer->cond);
    mutex_unlock(&peer->lock);
    return rc;
}

static int inproc_listener_init(int cd) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);
    struct ssmap_node *node = &cn->listener_node;

    node->key = cn->sock;
    node->keylen = strlen(cn->sock);
    INIT_LIST_HEAD(&cn->new_connectors);
    if ((rc = insert_listener(node)) < 0)
	return rc;
    return rc;
}


static int inproc_listener_destroy(int cd) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);
    struct channel *new;

    /* Avoiding the new connectors */
    remove_listener(&cn->listener_node);

    while ((new = pop_new_connector(cn))) {
	mutex_lock(&new->lock);
	/* TODO: Sending a ECONNREFUSED signel to the other peer. */
	if (--new->waiters == 0)
	    condition_signal(&new->cond);
	mutex_unlock(&new->lock);
    }
    return rc;
}


static int inproc_connector_init(int cd) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);
    struct channel *listener = find_listener(cn->peer);

    errno = ENOENT;
    if (!listener)
	return -1;
    cn->waiters = 0;
    cn->peer_channel = NULL;

    /* Push the new connector into listener's new_connectors queue */
    push_new_connector(listener, cn);

    mutex_lock(&cn->lock);
    if (++cn->waiters == 1)
	condition_wait(&cn->cond, &cn->lock);
    mutex_unlock(&cn->lock);
    assert(cn->waiters == 0 || cn->waiters == -1);

    /* If the other peer close the connection before the ESTABLISHED */
    if (!cn->peer_channel) {
	errno = ECONNREFUSED;
	return -1;
    }
    return rc;
}

static int inproc_connector_destroy(int cd) {
    return 0;
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

    /* Destroy the channel and free channel id. */
    free_channel(cn);
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


static struct channel_msg *channel_pop_rcvmsg(struct channel *cn) {
    struct channel_msg_item *mi = NULL;
    struct channel_msg *msg = NULL;

    if (!list_empty(&cn->rcv_head)) {
	mi = list_first(&cn->rcv_head, struct channel_msg_item, item);
	list_del_init(&mi->item);
	msg = &mi->msg;
    }
    return msg;
}


static int channel_push_sndmsg(struct channel *peer, struct channel_msg *msg) {
    int rc = 0;
    struct channel_msg_item *mi = cont_of(msg, struct channel_msg_item, msg);
    list_add_tail(&mi->item, &peer->rcv_head);
    return rc;
}


static int inproc_channel_recv(int cd, struct channel_msg **msg) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    mutex_lock(&cn->lock);
    while (!(*msg = channel_pop_rcvmsg(cn)) && !cn->fasync) {
	cn->waiters++;
	condition_wait(&cn->cond, &cn->lock);
	cn->waiters--;
    }
    cn->rcv--;
    if (cn->waiters)
	condition_signal(&cn->cond);
    mutex_unlock(&cn->lock);
    if (!*msg)
	rc = -EAGAIN;
    return rc;
}

static int inproc_channel_send(int cd, struct channel_msg *msg) {
    int rc = 0;
    struct channel *peer = cid_to_channel(cd)->peer_channel;

    mutex_lock(&peer->lock);
    while ((rc = channel_push_sndmsg(peer, msg)) < 0 && errno == EAGAIN) {
	peer->waiters++;
	condition_wait(&peer->cond, &peer->lock);
	peer->waiters--;
    }
    peer->rcv++;
    if (peer->waiters)
	condition_signal(&peer->cond);
    mutex_unlock(&peer->lock);
    return rc;
}

static struct channel_vf inproc_channel_vf = {
    .init = inproc_channel_init,
    .destroy = inproc_channel_destroy,
    .recv = inproc_channel_recv,
    .send = inproc_channel_send,
    .setopt = inproc_channel_setopt,
    .getopt = inproc_channel_getopt,
};

struct channel_vf *inproc_channel_vfptr = &inproc_channel_vf;
