#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "channel.h"
#include "channel_base.h"

extern struct channel_global cn_global;

extern struct channel *cid_to_channel(int cd);
extern void global_put_closing_channel(struct channel *cn);
extern int alloc_cid();

extern void generic_channel_init(int cd, int fd,
    struct transport *tp, struct channel_vf *vfptr);

static int64_t io_channel_read(struct io *io_ops, char *buff, int64_t sz) {
    int rc;
    struct channel *cn = cont_of(io_ops, struct channel, sock_ops);
    rc = cn->tp->read(cn->fd, buff, sz);
    return rc;
}

static int64_t io_channel_write(struct io *io_ops, char *buff, int64_t sz) {
    int rc;
    struct channel *cn = cont_of(io_ops, struct channel, sock_ops);
    rc = cn->tp->write(cn->fd, buff, sz);
    return rc;
}

static struct io default_channel_ops = {
    .read = io_channel_read,
    .write = io_channel_write,
};

static int accept_handler(epoll_t *el, epollevent_t *et);
static int io_handler(epoll_t *el, epollevent_t *et);

static void io_channel_init(int cd) {
    struct channel *cn = cid_to_channel(cd);

    cn->et.f = cn->faccepter ? accept_handler : io_handler;
    cn->sock_ops = default_channel_ops;
}

static void io_channel_destroy(int cd) {

}

static int io_channel_accept(int cd) {
    int new_cd, s;
    int nb = 1;
    struct channel *cn = cid_to_channel(cd);
    struct transport *tp = cn->tp;

    if ((s = tp->accept(cn->fd)) < 0)
	return s;
    tp->setopt(s, TP_NOBLOCK, &nb, sizeof(nb));

    // Find a unused channel id and slot.
    new_cd = alloc_cid();

    // Init channel from raw-level sockid and transport vfptr.
    generic_channel_init(new_cd, s, tp, io_channel_vfptr);
    return new_cd;
}

static void io_channel_close(int cd) {
    global_put_closing_channel(cid_to_channel(cd));
}

static int io_channel_setopt(int cd, int opt, void *val, int valsz) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    mutex_lock(&cn->lock);
    mutex_unlock(&cn->lock);
    return rc;
}

static int io_channel_getopt(int cd, int opt, void *val, int valsz) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    mutex_lock(&cn->lock);
    mutex_unlock(&cn->lock);
    return rc;
}

static void channel_push_rcvmsg(struct channel *cn, struct channel_msg *msg) {
    struct channel_msg_item *mi = cont_of(msg, struct channel_msg_item, msg);

    mutex_lock(&cn->lock);
    list_add_tail(&mi->item, &cn->rcv_head);

    /* Wakeup the blocking waiters. fix by needed here */
    condition_broadcast(&cn->cond);
    mutex_unlock(&cn->lock);
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


static int channel_push_sndmsg(struct channel *cn, struct channel_msg *msg) {
    int rc = 0;
    struct channel_msg_item *mi = cont_of(msg, struct channel_msg_item, msg);
    list_add_tail(&mi->item, &cn->snd_head);
    return rc;
}

static struct channel_msg *channel_pop_sndmsg(struct channel *cn) {
    struct channel_msg_item *mi;
    struct channel_msg *msg = NULL;
    
    mutex_lock(&cn->lock);
    if (!list_empty(&cn->snd_head)) {
	mi = list_first(&cn->snd_head, struct channel_msg_item, item);
	list_del_init(&mi->item);
	msg = &mi->msg;

	/* Wakeup the blocking waiters. fix by needed here */
	condition_broadcast(&cn->cond);
    }
    mutex_unlock(&cn->lock);

    return msg;
}

static int io_channel_recv(int cd, struct channel_msg **msg) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    mutex_lock(&cn->lock);
    while (!(*msg = channel_pop_rcvmsg(cn)) && !cn->fasync)
	condition_wait(&cn->cond, &cn->lock);
    mutex_unlock(&cn->lock);
    if (!*msg)
	rc = -EAGAIN;
    return rc;
}

static int io_channel_send(int cd, struct channel_msg *msg) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    mutex_lock(&cn->lock);
    while ((rc = channel_push_sndmsg(cn, msg)) < 0 && rc != -EAGAIN)
	condition_wait(&cn->cond, &cn->lock);
    mutex_unlock(&cn->lock);
    return rc;
}




static int accept_handler(epoll_t *el, epollevent_t *et) {
    int rc = 0;
    return rc;
}


static int msg_ready(struct bio *b, int64_t *payload_sz, int64_t *control_sz) {
    struct channel_msg_item mi = {};
    
    if (b->bsize < sizeof(mi.hdr))
	return false;
    bio_copy(b, (char *)(&mi.hdr), sizeof(mi.hdr));
    if (b->bsize < channel_msgiov_len(&mi.msg))
	return false;
    *payload_sz = mi.hdr.payload_sz;
    *control_sz = mi.hdr.control_sz;
    return true;
}

static void global_channel_poll_state(struct channel *cn) {
    struct list_head *err_head = &cn_global.error_head[cn->pd];
    struct list_head *in_head = &cn_global.readyin_head[cn->pd];
    struct list_head *out_head = &cn_global.readyout_head[cn->pd];
    
    mutex_lock(&cn_global.lock);
    mutex_lock(&cn->lock);

    /* Check the error status */
    if (!cn->fok && !attached(&cn->err_link))
	list_add_tail(&cn->err_link, err_head);

    /* Check the readyin status */
    if (attached(&cn->in_link) && list_empty(&cn->rcv_head))
	list_del_init(&cn->in_link);
    else if (!attached(&cn->in_link) && !list_empty(&cn->rcv_head))
	list_add_tail(&cn->in_link, in_head);

    /* Check the readyout status */
    if (attached(&cn->out_link) && cn->out.bsize > cn->snd_bufsz)
	list_del_init(&cn->out_link);
    else if (!attached(&cn->out_link) && cn->out.bsize <= cn->snd_bufsz)
	list_add_tail(&cn->out_link, out_head);
    
    mutex_unlock(&cn->lock);
    mutex_unlock(&cn_global.lock);
}

static int io_handler(epoll_t *el, epollevent_t *et) {
    int rc = 0;
    struct channel *cn = cont_of(et, struct channel, et);
    struct channel_msg *msg;
    int64_t payload_sz = 0, control_sz = 0;

    if (et->events & EPOLLIN) {
	if ((rc = bio_prefetch(&cn->in, &cn->sock_ops)) < 0 && errno != EAGAIN)
	    goto EXIT;
	while (msg_ready(&cn->in, &payload_sz, &control_sz)) {
	    msg = channel_allocmsg(payload_sz, control_sz);
	    bio_read(&cn->in, channel_msgiov_base(msg), channel_msgiov_len(msg));
	    channel_push_rcvmsg(cn, msg);
	}
    }
    if (et->events & EPOLLOUT) {
	while ((msg = channel_pop_sndmsg(cn)) != NULL) {
	    bio_write(&cn->out, channel_msgiov_base(msg), channel_msgiov_len(msg));
	    channel_freemsg(msg);
	}
	if ((rc = bio_flush(&cn->out, &cn->sock_ops)) < 0 && errno != EAGAIN)
	    goto EXIT;
    }
    if ((rc < 0 && errno != EAGAIN) || et->events & (EPOLLERR|EPOLLRDHUP))
	cn->fok = false;
    global_channel_poll_state(cn);
 EXIT:
    return rc;
}



static struct channel_vf io_channel_vf = {
    .init = io_channel_init,
    .destroy = io_channel_destroy,
    .accept = io_channel_accept,
    .close = io_channel_close,
    .recv = io_channel_recv,
    .send = io_channel_send,
    .setopt = io_channel_setopt,
    .getopt = io_channel_getopt,
};

struct channel_vf *io_channel_vfptr = &io_channel_vf;
