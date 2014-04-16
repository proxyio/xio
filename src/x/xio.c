#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "xbase.h"

extern struct xglobal xglobal;

extern struct xsock *cid_to_channel(int cd);
extern struct xtaskor *pid_to_xtaskor(int pd);
extern void xsock_free(struct xsock *cn);

extern struct xmsg *pop_rcv(struct xsock *cn);
extern void push_rcv(struct xsock *cn, struct xmsg *msg);
extern struct xmsg *pop_snd(struct xsock *cn);
extern int push_snd(struct xsock *cn, struct xmsg *msg);

extern void xpoll_notify(struct xsock *cn, u32 vf_spec);



static int64_t io_xread(struct io *ops, char *buff, int64_t sz) {
    struct xsock *cn = cont_of(ops, struct xsock, sock.ops);
    struct transport *tp = cn->sock.tp;
    int rc = tp->read(cn->sock.fd, buff, sz);
    return rc;
}

static int64_t io_xwrite(struct io *ops, char *buff, int64_t sz) {
    struct xsock *cn = cont_of(ops, struct xsock, sock.ops);
    struct transport *tp = cn->sock.tp;
    int rc = tp->write(cn->sock.fd, buff, sz);
    return rc;
}

static struct io default_xops = {
    .read = io_xread,
    .write = io_xwrite,
};



/******************************************************************************
 *  snd_head events trigger.
 ******************************************************************************/

static void snd_empty_event(int cd) {
    struct xsock *cn = cid_to_channel(cd);
    struct xtaskor *po = pid_to_xtaskor(cn->pollid);

    // Disable POLLOUT event when snd_head is empty
    if (cn->sock.et.events & EPOLLOUT) {
	DEBUG_OFF("%d disable EPOLLOUT", cd);
	cn->sock.et.events &= ~EPOLLOUT;
	BUG_ON(eloop_mod(&po->el, &cn->sock.et) != 0);
    }
}

static void snd_nonempty_event(int cd) {
    struct xsock *cn = cid_to_channel(cd);
    struct xtaskor *po = pid_to_xtaskor(cn->pollid);

    // Enable POLLOUT event when snd_head isn't empty
    if (!(cn->sock.et.events & EPOLLOUT)) {
	DEBUG_OFF("%d enable EPOLLOUT", cd);
	cn->sock.et.events |= EPOLLOUT;
	BUG_ON(eloop_mod(&po->el, &cn->sock.et) != 0);
    }
}


/******************************************************************************
 *  rcv_head events trigger.
 ******************************************************************************/

static void rcv_pop_event(int cd) {
    struct xsock *cn = cid_to_channel(cd);

    if (cn->snd_waiters)
	condition_signal(&cn->cond);
}

static void rcv_full_event(int cd) {
    struct xsock *cn = cid_to_channel(cd);    
    struct xtaskor *po = pid_to_xtaskor(cn->pollid);

    // Enable POLLOUT event when snd_head isn't empty
    if ((cn->sock.et.events & EPOLLIN)) {
	cn->sock.et.events &= ~EPOLLIN;
	BUG_ON(eloop_mod(&po->el, &cn->sock.et) != 0);
    }
}

static void rcv_nonfull_event(int cd) {
    struct xsock *cn = cid_to_channel(cd);    
    struct xtaskor *po = pid_to_xtaskor(cn->pollid);

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
    struct xsock *cn = cid_to_channel(cd);
    struct xtaskor *po = pid_to_xtaskor(cn->pollid);
    struct transport *tp = transport_lookup(cn->pf);
    struct xsock *parent = cid_to_channel(cn->parent);

    if ((s = tp->accept(parent->sock.fd)) < 0)
	return s;
    tp->setopt(s, TP_NOBLOCK, &on, sizeof(on));
    cn->sock.et.events = EPOLLIN|EPOLLRDHUP|EPOLLERR;
    cn->sock.et.fd = s;
    cn->sock.et.f = io_handler;
    cn->sock.et.data = cn;
    cn->sock.fd = s;
    cn->sock.tp = tp;
    cn->sock.ops = default_xops;
    BUG_ON(eloop_add(&po->el, &cn->sock.et) != 0);
    return rc;
}

static int io_listener_init(int cd) {
    int rc = 0;
    int s;
    int on = 1;
    struct xsock *cn = cid_to_channel(cd);
    struct xtaskor *po = pid_to_xtaskor(cn->pollid);
    struct transport *tp = transport_lookup(cn->pf);

    if ((s = tp->bind(cn->addr)) < 0)
	return s;
    tp->setopt(s, TP_NOBLOCK, &on, sizeof(on));
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
    struct xsock *cn = cid_to_channel(cd);
    struct xtaskor *po = pid_to_xtaskor(cn->pollid);
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
    cn->sock.ops = default_xops;
    BUG_ON(eloop_add(&po->el, &cn->sock.et) != 0);
    return rc;
}

static int io_xinit(int cd) {
    struct xsock *cn = cid_to_channel(cd);

    bio_init(&cn->sock.in);
    bio_init(&cn->sock.out);

    switch (cn->ty) {
    case XACCEPTER:
	return io_accepter_init(cd);
    case XCONNECTOR:
	return io_connector_init(cd);
    case XLISTENER:
	return io_listener_init(cd);
    }
    return -EINVAL;
}

static int io_snd(struct xsock *cn);

static void io_xdestroy(int cd) {
    struct xsock *cn = cid_to_channel(cd);
    struct xtaskor *po = pid_to_xtaskor(cn->pollid);
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
    xsock_free(cn);
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
    struct xsock *cn = cont_of(et, struct xsock, sock.et);

    if (et->happened & EPOLLERR)
	cn->fok = false;
    /* A new connection */
    else if (et->happened & EPOLLIN) {
	DEBUG_OFF("channel listener %d events %d", cn->cd, et->happened);
	xpoll_notify(cn, XPOLLIN);
    }
    return rc;
}


static int msg_ready(struct bio *b, int64_t *payload_sz) {
    struct xmsg msg = {};
    
    if (b->bsize < sizeof(msg.hdr))
	return false;
    bio_copy(b, (char *)(&msg.hdr), sizeof(msg.hdr));
    if (b->bsize < msg_iovlen(msg.hdr.payload))
	return false;
    *payload_sz = msg.hdr.size;
    return true;
}

static int io_rcv(struct xsock *cn) {
    int rc = 0;
    char *payload;
    int64_t payload_sz;
    struct xmsg *msg;

    rc = bio_prefetch(&cn->sock.in, &cn->sock.ops);
    if (rc < 0 && errno != EAGAIN)
	return rc;
    while (msg_ready(&cn->sock.in, &payload_sz)) {
	DEBUG_OFF("%d channel recv one message", cn->cd);
	payload = xallocmsg(payload_sz);
	bio_read(&cn->sock.in, msg_iovbase(payload), msg_iovlen(payload));
	msg = cont_of(payload, struct xmsg, hdr.payload);
	push_rcv(cn, msg);
    }
    return rc;
}

static int io_snd(struct xsock *cn) {
    int rc;
    char *payload;
    struct xmsg *msg;

    while ((msg = pop_snd(cn))) {
	payload = msg->hdr.payload;
	bio_write(&cn->sock.out, msg_iovbase(payload), msg_iovlen(payload));
	xfreemsg(payload);
    }
    rc = bio_flush(&cn->sock.out, &cn->sock.ops);
    return rc;
}

static int io_handler(eloop_t *el, ev_t *et) {
    int rc = 0;
    struct xsock *cn = cont_of(et, struct xsock, sock.et);

    if (et->happened & EPOLLIN) {
	DEBUG_OFF("io channel %d EPOLLIN", cn->cd);
	rc = io_rcv(cn);
    }
    if (et->happened & EPOLLOUT) {
	DEBUG_OFF("io channel %d EPOLLOUT", cn->cd);
	rc = io_snd(cn);
    }
    if ((rc < 0 && errno != EAGAIN) || et->happened & (EPOLLERR|EPOLLRDHUP)) {
	DEBUG_OFF("io channel %d EPIPE", cn->cd);
	cn->fok = false;
    }

    /* Check events for xpoll */
    xpoll_notify(cn, 0);
    return rc;
}



static struct xsock_vf tcp_xsock_vf = {
    .pf = PF_NET,
    .init = io_xinit,
    .destroy = io_xdestroy,
    .rcv_notify = io_rcv_notify,
    .snd_notify = io_snd_notify,
};

static struct xsock_vf ipc_xsock_vf = {
    .pf = PF_IPC,
    .init = io_xinit,
    .destroy = io_xdestroy,
    .rcv_notify = io_rcv_notify,
    .snd_notify = io_snd_notify,
};


struct xsock_vf *tcp_xsock_vfptr = &tcp_xsock_vf;
struct xsock_vf *ipc_xsock_vfptr = &ipc_xsock_vf;
