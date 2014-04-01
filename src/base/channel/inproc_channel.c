#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "channel.h"
#include "channel_base.h"

extern struct channel_global cn_global;

extern struct channel *cid_to_channel(int cd);
extern int alloc_cid();
extern int select_a_poller(int cd);
extern epoll_t *pid_to_poller(int pd);

static int inproc_accepter_init(int cd) {
    int rc = 0;
    return rc;
}

static int inproc_listener_init(int cd) {
    int rc = 0;
    return rc;
}

static int inproc_connector_init(int cd) {
    int rc = 0;
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

static int inproc_channel_recv(int cd, struct channel_msg **msg) {
    int rc = 0;
    return rc;
}

static int inproc_channel_send(int cd, struct channel_msg *msg) {
    int rc = 0;
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
