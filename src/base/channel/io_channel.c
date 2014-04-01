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
extern int select_a_poller(int cd);
extern struct channel_poll *pid_to_channel_poll(int pd);

static int64_t io_channel_read(struct io *io_ops, char *buff, int64_t sz) {
    struct channel *cn = cont_of(io_ops, struct channel, sock_ops);
    int rc = cn->tp->read(cn->fd, buff, sz);
    return rc;
}

static int64_t io_channel_write(struct io *io_ops, char *buff, int64_t sz) {
    struct channel *cn = cont_of(io_ops, struct channel, sock_ops);
    int rc = cn->tp->write(cn->fd, buff, sz);
    return rc;
}

static struct io default_channel_ops = {
    .read = io_channel_read,
    .write = io_channel_write,
};

static int accept_handler(eloop_t *el, ev_t *et);
static int io_handler(eloop_t *el, ev_t *et);


static int io_accepter_init(int cd) {
    int rc = 0;
    int s;
    int fnb = 1;
    struct channel *cn = cid_to_channel(cd);
    struct channel_poll *po;
    struct transport *tp = transport_lookup(cn->pf);
    struct channel *parent = cid_to_channel(cn->parent);

    if ((s = tp->accept(parent->fd)) < 0)
	return s;
    tp->setopt(s, TP_NOBLOCK, &fnb, sizeof(fnb));
    cn->et.events = EPOLLIN|EPOLLOUT|EPOLLRDHUP|EPOLLERR;
    cn->et.fd = s;
    cn->et.f = io_handler;
    cn->et.data = cn;
    cn->pollid = select_a_poller(cd);
    po = pid_to_channel_poll(cn->pollid);
    cn->fd = s;
    cn->tp = tp;
    cn->sock_ops = default_channel_ops;
    assert(eloop_add(&po->el, &cn->et) == 0);
    return rc;
}

static int io_listener_init(int cd) {
    int rc = 0;
    int s;
    struct channel *cn = cid_to_channel(cd);
    struct channel_poll *po;
    struct transport *tp = transport_lookup(cn->pf);

    if ((s = tp->bind(cn->sock)) < 0)
	return s;
    cn->et.events = EPOLLIN|EPOLLERR;
    cn->et.fd = s;
    cn->et.f = accept_handler;
    cn->et.data = cn;
    cn->pollid = select_a_poller(cd);
    po = pid_to_channel_poll(cn->pollid);
    cn->fd = s;
    cn->tp = tp;
    assert(eloop_add(&po->el, &cn->et) == 0);
    return rc;
}

static int io_connector_init(int cd) {
    int rc = 0;
    int s;
    int fnb = 1;
    struct channel *cn = cid_to_channel(cd);
    struct channel_poll *po;
    struct transport *tp = transport_lookup(cn->pf);

    if ((s = tp->connect(cn->peer)) < 0)
	return s;
    tp->setopt(s, TP_NOBLOCK, &fnb, sizeof(fnb));
    cn->et.events = EPOLLIN|EPOLLOUT|EPOLLRDHUP|EPOLLERR;
    cn->et.fd = s;
    cn->et.f = io_handler;
    cn->et.data = cn;
    cn->pollid = select_a_poller(cd);
    po = pid_to_channel_poll(cn->pollid);
    cn->fd = s;
    cn->tp = tp;
    cn->sock_ops = default_channel_ops;
    assert(eloop_add(&po->el, &cn->et) == 0);
    return rc;
}

static int io_channel_init(int cd) {
    struct channel *cn = cid_to_channel(cd);

    bio_init(&cn->in);
    bio_init(&cn->out);

    INIT_LIST_HEAD(&cn->closing_link);
    INIT_LIST_HEAD(&cn->err_link);
    INIT_LIST_HEAD(&cn->in_link);
    INIT_LIST_HEAD(&cn->out_link);
    
    switch (cn->ty) {
    case CHANNEL_ACCEPTER:
	return io_accepter_init(cd);
    case CHANNEL_CONNECTOR:
	return io_connector_init(cd);
    case CHANNEL_LISTENER:
	return io_listener_init(cd);
    }
    return -EINVAL;
}

static void io_channel_destroy(int cd) {
    struct channel *cn = cid_to_channel(cd);
    struct channel_poll *po = pid_to_channel_poll(cn->pollid);
    struct transport *tp = cn->tp;
    
    /* Detach channel low-level file descriptor from poller */
    assert(eloop_del(&po->el, &cn->et) == 0);
    tp->close(cn->fd);

    cn->fd = -1;
    cn->et.events = -1;
    cn->et.fd = -1;
    cn->et.f = 0;
    cn->et.data = 0;
    cn->pollid = -1;
    cn->tp = 0;

    /* Detach from all poll status head */
    if (attached(&cn->closing_link))
	list_del_init(&cn->closing_link);
    if (attached(&cn->err_link))
	list_del_init(&cn->err_link);
    if (attached(&cn->in_link))
	list_del_init(&cn->in_link);
    if (attached(&cn->out_link))
	list_del_init(&cn->out_link);
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

    /* Wakeup the blocking waiters. */
    if (cn->waiters > 0)
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

	/* Wakeup the blocking waiters */
	if (cn->waiters > 0)
	    condition_broadcast(&cn->cond);
    }
    mutex_unlock(&cn->lock);

    return msg;
}

static int io_channel_recv(int cd, struct channel_msg **msg) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    mutex_lock(&cn->lock);
    while (!(*msg = channel_pop_rcvmsg(cn)) && !cn->fasync) {
	cn->waiters++;
	condition_wait(&cn->cond, &cn->lock);
	cn->waiters--;
    }
    mutex_unlock(&cn->lock);
    if (!*msg)
	rc = -EAGAIN;
    return rc;
}

static int io_channel_send(int cd, struct channel_msg *msg) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    mutex_lock(&cn->lock);
    while ((rc = channel_push_sndmsg(cn, msg)) < 0 && rc != -EAGAIN) {
	cn->waiters++;
	condition_wait(&cn->cond, &cn->lock);
	cn->waiters--;
    }
    mutex_unlock(&cn->lock);
    return rc;
}




static int accept_handler(eloop_t *el, ev_t *et) {
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

static int io_handler(eloop_t *el, ev_t *et) {
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
 EXIT:
    return rc;
}



static struct channel_vf io_channel_vf = {
    .init = io_channel_init,
    .destroy = io_channel_destroy,
    .recv = io_channel_recv,
    .send = io_channel_send,
    .setopt = io_channel_setopt,
    .getopt = io_channel_getopt,
};

struct channel_vf *io_channel_vfptr = &io_channel_vf;
