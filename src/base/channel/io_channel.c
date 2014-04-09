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

extern struct channel_msg *pop_rcv(struct channel *cn);
extern void push_rcv(struct channel *cn, struct channel_msg *msg);
extern struct channel_msg *pop_snd(struct channel *cn);
extern int push_snd(struct channel *cn, struct channel_msg *msg);

extern void update_upoll_tb(struct channel *cn);



static int64_t io_channel_read(struct io *ops, char *buff, int64_t sz) {
    struct channel *cn = cont_of(ops, struct channel, sock.ops);
    struct transport *tp = cn->sock.tp;
    int rc = tp->read(cn->sock.fd, buff, sz);
    return rc;
}

static int64_t io_channel_write(struct io *ops, char *buff, int64_t sz) {
    struct channel *cn = cont_of(ops, struct channel, sock.ops);
    struct transport *tp = cn->sock.tp;
    int rc = tp->write(cn->sock.fd, buff, sz);
    return rc;
}

static struct io default_channel_ops = {
    .read = io_channel_read,
    .write = io_channel_write,
};



/******************************************************************************
 *  snd_head events trigger.
 ******************************************************************************/

static void snd_empty_event(int cd) {
    struct channel *cn = cid_to_channel(cd);
    struct channel_poll *po = pid_to_channel_poll(cn->pollid);

    // Disable POLLOUT event when snd_head is empty
    if (cn->sock.et.events & EPOLLOUT) {
	cn->sock.et.events &= ~EPOLLOUT;
	BUG_ON(eloop_mod(&po->el, &cn->sock.et) != 0);
    }
}

static void snd_nonempty_event(int cd) {
    struct channel *cn = cid_to_channel(cd);
    struct channel_poll *po = pid_to_channel_poll(cn->pollid);

    // Enable POLLOUT event when snd_head isn't empty
    if (!(cn->sock.et.events & EPOLLOUT)) {
	cn->sock.et.events |= EPOLLOUT;
	BUG_ON(eloop_mod(&po->el, &cn->sock.et) != 0);
    }
}


/******************************************************************************
 *  rcv_head events trigger.
 ******************************************************************************/

static void rcv_pop_event(int cd) {
    struct channel *cn = cid_to_channel(cd);

    if (cn->snd_waiters)
	condition_signal(&cn->cond);
}

static void rcv_full_event(int cd) {
    struct channel *cn = cid_to_channel(cd);    
    struct channel_poll *po = pid_to_channel_poll(cn->pollid);

    // Enable POLLOUT event when snd_head isn't empty
    if ((cn->sock.et.events & EPOLLIN)) {
	cn->sock.et.events &= ~EPOLLIN;
	BUG_ON(eloop_mod(&po->el, &cn->sock.et) != 0);
    }
}

static void rcv_nonfull_event(int cd) {
    struct channel *cn = cid_to_channel(cd);    
    struct channel_poll *po = pid_to_channel_poll(cn->pollid);

    // Enable POLLOUT event when snd_head isn't empty
    if (!(cn->sock.et.events & EPOLLIN)) {
	cn->sock.et.events |= EPOLLIN;
	BUG_ON(eloop_mod(&po->el, &cn->sock.et) != 0);
    }
}





static int accept_handler(eloop_t *el, ev_t *et);
static int io_handler(eloop_t *el, ev_t *et);

static int io_accepter_init(int cd) {
    int rc = 0;
    int s;
    int on = 1;
    struct channel *cn = cid_to_channel(cd);
    struct channel_poll *po = pid_to_channel_poll(cn->pollid);
    struct transport *tp = transport_lookup(cn->pf);
    struct channel *parent = cid_to_channel(cn->parent);

    if ((s = tp->accept(parent->sock.fd)) < 0)
	return s;
    tp->setopt(s, TP_NOBLOCK, &on, sizeof(on));
    cn->sock.et.events = EPOLLIN|EPOLLRDHUP|EPOLLERR;
    cn->sock.et.fd = s;
    cn->sock.et.f = io_handler;
    cn->sock.et.data = cn;
    cn->sock.fd = s;
    cn->sock.tp = tp;
    cn->sock.ops = default_channel_ops;
    BUG_ON(eloop_add(&po->el, &cn->sock.et) != 0);
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
    // TODO: async accept the new connection by using EPOLLIN
    cn->sock.et.events = EPOLLIN|EPOLLERR;
    cn->sock.et.fd = s;
    cn->sock.et.f = accept_handler;
    cn->sock.et.data = cn;
    cn->sock.fd = s;
    cn->sock.tp = tp;
    BUG_ON(eloop_add(&po->el, &cn->sock.et) != 0);
    return rc;
}

static int io_connector_init(int cd) {
    int rc = 0;
    int s;
    int on = 1;
    struct channel *cn = cid_to_channel(cd);
    struct channel_poll *po = pid_to_channel_poll(cn->pollid);
    struct transport *tp = transport_lookup(cn->pf);

    if ((s = tp->connect(cn->peer)) < 0)
	return s;
    tp->setopt(s, TP_NOBLOCK, &on, sizeof(on));
    cn->sock.et.events = EPOLLIN|EPOLLRDHUP|EPOLLERR;
    cn->sock.et.fd = s;
    cn->sock.et.f = io_handler;
    cn->sock.et.data = cn;
    cn->sock.fd = s;
    cn->sock.tp = tp;
    cn->sock.ops = default_channel_ops;
    BUG_ON(eloop_add(&po->el, &cn->sock.et) != 0);
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

static int io_snd(struct channel *cn);

static void io_channel_destroy(int cd) {
    struct channel *cn = cid_to_channel(cd);
    struct channel_poll *po = pid_to_channel_poll(cn->pollid);
    struct transport *tp = cn->sock.tp;

    /* Try flush buf massage into network before close */
    io_snd(cn);

    /* Detach channel low-level file descriptor from poller */
    BUG_ON(eloop_del(&po->el, &cn->sock.et) != 0);
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


static void io_rcv_notify(int cd, uint32_t events) {
    if (events & MQ_POP)
	rcv_pop_event(cd);
    if (events & MQ_FULL)
	rcv_full_event(cd);
    else if (events & MQ_NONFULL)
	rcv_nonfull_event(cd);
}

static void io_snd_notify(int cd, uint32_t events) {
    if (events & MQ_EMPTY)
	snd_empty_event(cd);
    else if (events & MQ_NONEMPTY)
	snd_nonempty_event(cd);
}


static int accept_handler(eloop_t *el, ev_t *et) {
    int rc = 0;
    struct channel *cn = cont_of(et, struct channel, sock.et);

    if (et->happened & EPOLLERR)
	cn->fok = false;
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

static int io_rcv(struct channel *cn) {
    int rc = 0;
    char *payload;
    int64_t payload_sz;
    struct channel_msg *msg;

    if ((rc = bio_prefetch(&cn->sock.in, &cn->sock.ops)) < 0 && errno != EAGAIN)
	return rc;
    while (msg_ready(&cn->sock.in, &payload_sz)) {
	payload = channel_allocmsg(payload_sz);
	bio_read(&cn->sock.in, msg_iovbase(payload), msg_iovlen(payload));
	msg = cont_of(payload, struct channel_msg, hdr.payload);
	push_rcv(cn, msg);
    }
    return rc;
}

static int io_snd(struct channel *cn) {
    int rc;
    char *payload;
    struct channel_msg *msg;

    while ((msg = pop_snd(cn))) {
	payload = msg->hdr.payload;
	bio_write(&cn->sock.out, msg_iovbase(payload), msg_iovlen(payload));
	channel_freemsg(payload);
    }
    rc = bio_flush(&cn->sock.out, &cn->sock.ops);
    return rc;
}

static int io_handler(eloop_t *el, ev_t *et) {
    int rc = 0;
    struct channel *cn = cont_of(et, struct channel, sock.et);

    if (et->happened & EPOLLIN) {
	rc = io_rcv(cn);
    }
    if (et->happened & EPOLLOUT) {
	rc = io_snd(cn);
    }
    if ((rc < 0 && errno != EAGAIN) || et->happened & (EPOLLERR|EPOLLRDHUP))
	cn->fok = false;

    /* Check events for upoll */
    update_upoll_tb(cn);
    return rc;
}



static struct channel_vf tcp_channel_vf = {
    .pf = PF_NET,
    .init = io_channel_init,
    .destroy = io_channel_destroy,
    .rcv_notify = io_rcv_notify,
    .snd_notify = io_snd_notify,
};

static struct channel_vf ipc_channel_vf = {
    .pf = PF_IPC,
    .init = io_channel_init,
    .destroy = io_channel_destroy,
    .rcv_notify = io_rcv_notify,
    .snd_notify = io_snd_notify,
};


struct channel_vf *tcp_channel_vfptr = &tcp_channel_vf;
struct channel_vf *ipc_channel_vfptr = &ipc_channel_vf;
