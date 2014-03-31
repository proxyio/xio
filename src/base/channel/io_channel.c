#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "channel.h"
#include "channel_base.h"

extern struct channel_global cn_global;

extern uint32_t channel_msgiov_len(struct channel_msg *msg);
extern char *channel_msgiov_base(struct channel_msg *msg);

extern int global_get_channel_id();
extern void global_put_channel_id(int cd);
extern struct channel *global_channel(int cd);

extern int global_get_poller_id();
extern void global_put_poller_id(int pd);
extern epoll_t *global_poller(int pd);
extern int select_a_poller(int cd);

extern void global_put_closing_channel(struct channel *cn);
extern struct channel *global_get_closing_channel(int pd);

extern void global_channel_poll_state(struct channel *cn);

extern void generic_channel_init(int cd, int isaccepter, int fd, struct transport *tp);
extern void generic_channel_destroy(struct channel *cn);

extern struct channel_msg *channel_allocmsg(uint32_t payload_sz, uint32_t control_sz);
extern void channel_freemsg(struct channel_msg *msg);


static int64_t io_channel_read(struct io *io_ops, char *buff, int64_t sz) {
    int rc;
    struct channel *cn = pio_cont(io_ops, struct channel, sock_ops);
    rc = cn->tp->read(cn->fd, buff, sz);
    return rc;
}

static int64_t io_channel_write(struct io *io_ops, char *buff, int64_t sz) {
    int rc;
    struct channel *cn = pio_cont(io_ops, struct channel, sock_ops);
    rc = cn->tp->write(cn->fd, buff, sz);
    return rc;
}

static struct io default_channel_ops = {
    .read = io_channel_read,
    .write = io_channel_write,
};

static int io_event_handler(epoll_t *el, epollevent_t *et);


static void io_channel_init(int cd) {
    struct channel *cn = global_channel(cd);

    cn->et.f = io_event_handler;
    cn->sock_ops = default_channel_ops;
}

static void io_channel_destroy(int cd) {

}

static int io_channel_accept(int cd) {
    return -EINVAL;
}

static void io_channel_close(int cd) {
    global_put_closing_channel(global_channel(cd));
}

static int io_channel_setopt(int cd, int opt, void *val, int valsz) {
    int rc = 0;
    struct channel *cn = global_channel(cd);
    struct transport *tp = cn->tp;

    mutex_lock(&cn->lock);

    if (opt == PIO_NONBLOCK) {
	if (!cn->fasync && (*(int *)val))
	    cn->fasync = true;
	else if (cn->fasync && (*(int *)val) == 0)
	    cn->fasync = false;
    } else
	rc = tp->setopt(cn->fd, opt, val, valsz);

    mutex_unlock(&cn->lock);
    return rc;
}

static int io_channel_getopt(int cd, int opt, void *val, int valsz) {
    int rc;
    struct channel *cn = global_channel(cd);
    struct transport *tp = cn->tp;

    mutex_lock(&cn->lock);

    rc = tp->getopt(cn->fd, opt, val, valsz);

    mutex_unlock(&cn->lock);
    return rc;
}

static void channel_push_rcvmsg(struct channel *cn, struct channel_msg *msg) {
    struct channel_msg_item *msgi = pio_cont(msg, struct channel_msg_item, msg);

    mutex_lock(&cn->lock);
    list_add_tail(&msgi->item, &cn->rcv_head);

    /* Wakeup the blocking waiters. fix by needed here */
    condition_broadcast(&cn->cond);
    mutex_unlock(&cn->lock);
}

static struct channel_msg *channel_pop_rcvmsg(struct channel *cn) {
    struct channel_msg_item *msgi = NULL;
    struct channel_msg *msg = NULL;

    if (!list_empty(&cn->rcv_head)) {
	msgi = list_first(&cn->rcv_head, struct channel_msg_item, item);
	list_del_init(&msgi->item);
	msg = &msgi->msg;
    }
    return msg;
}


static int channel_push_sndmsg(struct channel *cn, struct channel_msg *msg) {
    int rc = 0;
    struct channel_msg_item *msgi = pio_cont(msg, struct channel_msg_item, msg);
    list_add_tail(&msgi->item, &cn->snd_head);
    return rc;
}

static struct channel_msg *channel_pop_sndmsg(struct channel *cn) {
    struct channel_msg_item *msgi;
    struct channel_msg *msg = NULL;
    
    mutex_lock(&cn->lock);
    if (!list_empty(&cn->snd_head)) {
	msgi = list_first(&cn->snd_head, struct channel_msg_item, item);
	list_del_init(&msgi->item);
	msg = &msgi->msg;

	/* Wakeup the blocking waiters. fix by needed here */
	condition_broadcast(&cn->cond);
    }
    mutex_unlock(&cn->lock);

    return msg;
}

static int io_channel_recv(int cd, struct channel_msg **msg) {
    int rc = 0;
    struct channel *cn = global_channel(cd);

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
    struct channel *cn = global_channel(cd);

    mutex_lock(&cn->lock);
    while ((rc = channel_push_sndmsg(cn, msg)) < 0 && rc != -EAGAIN)
	condition_wait(&cn->cond, &cn->lock);
    mutex_unlock(&cn->lock);
    return rc;
}


static int msg_ready(struct bio *b, int64_t *payload_sz, int64_t *control_sz) {
    struct channel_msg_item msgi = {};
    
    if (b->bsize < sizeof(msgi.hdr))
	return false;
    bio_copy(b, (char *)(&msgi.hdr), sizeof(msgi.hdr));
    if (b->bsize < channel_msgiov_len(&msgi.msg))
	return false;
    *payload_sz = msgi.hdr.payload_sz;
    *control_sz = msgi.hdr.control_sz;
    return true;
}

static int io_event_handler(epoll_t *el, epollevent_t *et) {
    int rc = 0;
    struct channel *cn = pio_cont(et, struct channel, et);
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
