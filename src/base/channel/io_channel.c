#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "channel_base.h"

extern struct channel_global cn_global;

extern struct channel *cid_to_channel(int cd);
extern struct channel_poll *pid_to_channel_poll(int pd);
extern void free_channel(struct channel *cn);


static int64_t io_channel_read(struct io *io_ops, char *buff, int64_t sz) {
    struct channel *cn = cont_of(io_ops, struct channel, sock.ops);
    struct transport *tp = cn->sock.tp;
    int rc = tp->read(cn->sock.fd, buff, sz);
    return rc;
}

static int64_t io_channel_write(struct io *io_ops, char *buff, int64_t sz) {
    struct channel *cn = cont_of(io_ops, struct channel, sock.ops);
    struct transport *tp = cn->sock.tp;
    int rc = tp->write(cn->sock.fd, buff, sz);
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
    struct channel_poll *po = pid_to_channel_poll(cn->pollid);
    struct transport *tp = transport_lookup(cn->pf);
    struct channel *parent = cid_to_channel(cn->parent);

    if ((s = tp->accept(parent->sock.fd)) < 0)
	return s;
    tp->setopt(s, TP_NOBLOCK, &fnb, sizeof(fnb));
    cn->sock.et.events = EPOLLIN|EPOLLOUT|EPOLLRDHUP|EPOLLERR;
    cn->sock.et.fd = s;
    cn->sock.et.f = io_handler;
    cn->sock.et.data = cn;
    cn->sock.fd = s;
    cn->sock.tp = tp;
    cn->sock.ops = default_channel_ops;
    assert(eloop_add(&po->el, &cn->sock.et) == 0);
    return rc;
}

static int io_listener_init(int cd) {
    int rc = 0;
    int s;
    struct channel *cn = cid_to_channel(cd);
    struct channel_poll *po = pid_to_channel_poll(cn->pollid);
    struct transport *tp = transport_lookup(cn->pf);

    if ((s = tp->bind(cn->addr)) < 0)
	return s;
    cn->sock.et.events = EPOLLIN|EPOLLERR;
    cn->sock.et.fd = s;
    cn->sock.et.f = accept_handler;
    cn->sock.et.data = cn;
    cn->sock.fd = s;
    cn->sock.tp = tp;
    assert(eloop_add(&po->el, &cn->sock.et) == 0);
    return rc;
}

static int io_connector_init(int cd) {
    int rc = 0;
    int s;
    int fnb = 1;
    struct channel *cn = cid_to_channel(cd);
    struct channel_poll *po = pid_to_channel_poll(cn->pollid);
    struct transport *tp = transport_lookup(cn->pf);

    if ((s = tp->connect(cn->peer)) < 0)
	return s;
    tp->setopt(s, TP_NOBLOCK, &fnb, sizeof(fnb));
    cn->sock.et.events = EPOLLIN|EPOLLOUT|EPOLLRDHUP|EPOLLERR;
    cn->sock.et.fd = s;
    cn->sock.et.f = io_handler;
    cn->sock.et.data = cn;
    cn->sock.fd = s;
    cn->sock.tp = tp;
    cn->sock.ops = default_channel_ops;
    assert(eloop_add(&po->el, &cn->sock.et) == 0);
    return rc;
}

static int io_channel_init(int cd) {
    struct channel *cn = cid_to_channel(cd);

    bio_init(&cn->sock.in);
    bio_init(&cn->sock.out);

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
    struct transport *tp = cn->sock.tp;
    
    /* Detach channel low-level file descriptor from poller */
    assert(eloop_del(&po->el, &cn->sock.et) == 0);
    tp->close(cn->sock.fd);

    cn->sock.fd = -1;
    cn->sock.et.events = -1;
    cn->sock.et.fd = -1;
    cn->sock.et.f = 0;
    cn->sock.et.data = 0;
    cn->sock.tp = 0;

    /* Destroy the channel base and free channelid. */
    free_channel(cn);
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

static void channel_push_rcvmsg(struct channel *cn, char *payload) {
    struct channel_msg *msg = cont_of(payload, struct channel_msg, hdr.payload);

    mutex_lock(&cn->lock);
    list_add_tail(&msg->item, &cn->rcv_head);

    /* Wakeup the blocking waiters. */
    if (cn->rcv_waiters > 0)
	condition_broadcast(&cn->cond);
    mutex_unlock(&cn->lock);
}

static char *channel_pop_rcvmsg(struct channel *cn) {
    struct channel_msg *msg;

    if (!list_empty(&cn->rcv_head)) {
	msg = list_first(&cn->rcv_head, struct channel_msg, item);
	list_del_init(&msg->item);
	return msg->hdr.payload;
    }
    return NULL;
}


static int channel_push_sndmsg(struct channel *cn, char *payload) {
    int rc = 0;
    struct channel_msg *msg = cont_of(payload, struct channel_msg, hdr.payload);
    list_add_tail(&msg->item, &cn->snd_head);
    return rc;
}

static char *channel_pop_sndmsg(struct channel *cn) {
    char *payload = NULL;
    struct channel_msg *msg;
    
    mutex_lock(&cn->lock);
    if (!list_empty(&cn->snd_head)) {
	msg = list_first(&cn->snd_head, struct channel_msg, item);
	list_del_init(&msg->item);
	payload = msg->hdr.payload;

	/* Wakeup the blocking waiters */
	if (cn->snd_waiters > 0)
	    condition_broadcast(&cn->cond);
    }
    mutex_unlock(&cn->lock);

    return payload;
}

static int io_channel_recv(int cd, char **payload) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    mutex_lock(&cn->lock);
    while (!(*payload = channel_pop_rcvmsg(cn)) && !cn->fasync) {
	cn->rcv_waiters++;
	condition_wait(&cn->cond, &cn->lock);
	cn->rcv_waiters--;
    }
    mutex_unlock(&cn->lock);
    if (!*payload)
	rc = -EAGAIN;
    return rc;
}

static int io_channel_send(int cd, char *payload) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    mutex_lock(&cn->lock);
    while ((rc = channel_push_sndmsg(cn, payload)) < 0 && errno == EAGAIN) {
	cn->snd_waiters++;
	condition_wait(&cn->cond, &cn->lock);
	cn->snd_waiters--;
    }
    mutex_unlock(&cn->lock);
    return rc;
}




static int accept_handler(eloop_t *el, ev_t *et) {
    int rc = 0;
    return rc;
}


static int msg_ready(struct bio *b, int64_t *payload_sz) {
    struct channel_msg msg = {};
    
    if (b->bsize < sizeof(msg.hdr))
	return false;
    bio_copy(b, (char *)(&msg.hdr), sizeof(msg.hdr));
    if (b->bsize < msg_iovlen(msg.hdr.payload))
	return false;
    *payload_sz = msg.hdr.size;
    return true;
}

static int io_handler(eloop_t *el, ev_t *et) {
    int rc = 0;
    struct channel *cn = cont_of(et, struct channel, sock.et);
    char *payload;
    int64_t payload_sz = 0;

    if (et->events & EPOLLIN) {
	if ((rc = bio_prefetch(&cn->sock.in, &cn->sock.ops)) < 0 && errno != EAGAIN)
	    goto EXIT;
	while (msg_ready(&cn->sock.in, &payload_sz)) {
	    payload = channel_allocmsg(payload_sz);
	    bio_read(&cn->sock.in, msg_iovbase(payload), msg_iovlen(payload));
	    channel_push_rcvmsg(cn, payload);
	}
    }
    if (et->events & EPOLLOUT) {
	while ((payload = channel_pop_sndmsg(cn)) != NULL) {
	    bio_write(&cn->sock.out, msg_iovbase(payload), msg_iovlen(payload));
	    channel_freemsg(payload);
	}
	if ((rc = bio_flush(&cn->sock.out, &cn->sock.ops)) < 0 && errno != EAGAIN)
	    goto EXIT;
    }
    if ((rc < 0 && errno != EAGAIN) || et->events & (EPOLLERR|EPOLLRDHUP))
	cn->fok = false;
 EXIT:
    return rc;
}



static struct channel_vf tcp_channel_vf = {
    .pf = PF_NET,
    .init = io_channel_init,
    .destroy = io_channel_destroy,
    .recv = io_channel_recv,
    .send = io_channel_send,
    .setopt = io_channel_setopt,
    .getopt = io_channel_getopt,
};

static struct channel_vf ipc_channel_vf = {
    .pf = PF_IPC,
    .init = io_channel_init,
    .destroy = io_channel_destroy,
    .recv = io_channel_recv,
    .send = io_channel_send,
    .setopt = io_channel_setopt,
    .getopt = io_channel_getopt,
};


struct channel_vf *tcp_channel_vfptr = &tcp_channel_vf;
struct channel_vf *ipc_channel_vfptr = &ipc_channel_vf;
