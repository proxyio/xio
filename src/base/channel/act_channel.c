#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "channel.h"
#include "channel_base.h"

extern int global_get_channel_id();
extern struct channel *global_channel(int cd);
extern void global_put_closing_channel(struct channel *cn);
extern void generic_channel_init(int cd, int fd,
    struct transport *tp, struct channel_vf *vfptr);

static int act_event_handler(epoll_t *el, epollevent_t *et) {
    int rc = 0;
    return rc;
}

static void act_channel_init(int cd) {
    struct channel *cn = global_channel(cd);
    cn->et.f = act_event_handler;
}

static void act_channel_destroy(int cd) {

}

static void act_channel_close(int cd) {
    global_put_closing_channel(global_channel(cd));
}

static int act_channel_accept(int cd) {
    int new_cd, s;
    int ff = 1;
    struct channel *cn = global_channel(cd);
    struct transport *tp = cn->tp;

    if ((s = tp->accept(cn->fd)) < 0)
	return s;
    tp->setopt(s, TP_NOBLOCK, &ff, sizeof(ff));

    // Find a unused channel id and slot.
    new_cd = global_get_channel_id();

    // Init channel from raw-level sockid and transport vfptr.
    generic_channel_init(new_cd, s, tp, io_channel_vfptr);
    return new_cd;
}

static int act_channel_setopt(int cd, int opt, void *val, int valsz) {
    int rc = 0;
    struct channel *cn = global_channel(cd);

    mutex_lock(&cn->lock);
    mutex_unlock(&cn->lock);
    return rc;
}

static int act_channel_getopt(int cd, int opt, void *val, int valsz) {
    int rc = 0;
    struct channel *cn = global_channel(cd);

    mutex_lock(&cn->lock);
    mutex_unlock(&cn->lock);
    return rc;
}

static int act_channel_recv(int cd, struct channel_msg **msg) {
    return -EINVAL;
}

static int act_channel_send(int cd, struct channel_msg *msg) {
    return -EINVAL;
}


static struct channel_vf act_channel_vf = {
    .init = act_channel_init,
    .destroy = act_channel_destroy,
    .accept = act_channel_accept,
    .close = act_channel_close,
    .recv = act_channel_recv,
    .send = act_channel_send,
    .setopt = act_channel_setopt,
    .getopt = act_channel_getopt,
};

struct channel_vf *act_channel_vfptr = &act_channel_vf;
