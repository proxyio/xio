#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "xbase.h"

static i64 xio_connector_read(struct io *ops, char *buff, i64 sz) {
    struct xsock *sx = cont_of(ops, struct xsock, io.ops);
    struct transport *tp = sx->io.tp;

    BUG_ON(!tp);
    int rc = tp->read(sx->io.fd, buff, sz);
    return rc;
}

static i64 xio_connector_write(struct io *ops, char *buff, i64 sz) {
    struct xsock *sx = cont_of(ops, struct xsock, io.ops);
    struct transport *tp = sx->io.tp;

    BUG_ON(!tp);
    int rc = tp->write(sx->io.fd, buff, sz);
    return rc;
}

struct io default_xops = {
    .read = xio_connector_read,
    .write = xio_connector_write,
};



/******************************************************************************
 *  snd_head events trigger.
 ******************************************************************************/

static void snd_head_empty(int xd) {
    struct xsock *sx = xget(xd);
    struct xcpu *cpu = xcpuget(sx->cpu_no);

    // Disable POLLOUT event when snd_head is empty
    if (sx->io.et.events & EPOLLOUT) {
	DEBUG_ON("%d disable EPOLLOUT", xd);
	sx->io.et.events &= ~EPOLLOUT;
	BUG_ON(eloop_mod(&cpu->el, &sx->io.et) != 0);
    }
}

static void snd_head_nonempty(int xd) {
    struct xsock *sx = xget(xd);
    struct xcpu *cpu = xcpuget(sx->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if (!(sx->io.et.events & EPOLLOUT)) {
	DEBUG_ON("%d enable EPOLLOUT", xd);
	sx->io.et.events |= EPOLLOUT;
	BUG_ON(eloop_mod(&cpu->el, &sx->io.et) != 0);
    }
}


/******************************************************************************
 *  rcv_head events trigger.
 ******************************************************************************/

static void rcv_head_pop(int xd) {
    struct xsock *sx = xget(xd);

    if (sx->snd_waiters)
	condition_signal(&sx->cond);
}

static void rcv_head_full(int xd) {
    struct xsock *sx = xget(xd);    
    struct xcpu *cpu = xcpuget(sx->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if ((sx->io.et.events & EPOLLIN)) {
	sx->io.et.events &= ~EPOLLIN;
	BUG_ON(eloop_mod(&cpu->el, &sx->io.et) != 0);
    }
}

static void rcv_head_nonfull(int xd) {
    struct xsock *sx = xget(xd);    
    struct xcpu *cpu = xcpuget(sx->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if (!(sx->io.et.events & EPOLLIN)) {
	sx->io.et.events |= EPOLLIN;
	BUG_ON(eloop_mod(&cpu->el, &sx->io.et) != 0);
    }
}

int xio_connector_handler(eloop_t *el, ev_t *et);

static int xio_connector_init(int xd) {
    int rc = 0;
    int s;
    int on = 1;
    struct xsock *sx = xget(xd);
    struct xcpu *cpu = xcpuget(sx->cpu_no);
    struct transport *tp = transport_lookup(sx->pf);

    BUG_ON(!tp);
    ZERO(sx->io);
    bio_init(&sx->io.in);
    bio_init(&sx->io.out);

    if ((s = tp->connect(sx->peer)) < 0)
	return s;
    tp->setopt(s, TP_NOBLOCK, &on, sizeof(on));
    sx->io.et.events = EPOLLIN|EPOLLRDHUP|EPOLLERR;
    sx->io.et.fd = s;
    sx->io.et.f = xio_connector_handler;
    sx->io.et.data = sx;
    sx->io.fd = s;
    sx->io.tp = tp;
    sx->io.ops = default_xops;
    BUG_ON(eloop_add(&cpu->el, &sx->io.et) != 0);
    return rc;
}

static int xio_connector_snd(struct xsock *sx);

static void xio_connector_destroy(int xd) {
    struct xsock *sx = xget(xd);
    struct xcpu *cpu = xcpuget(sx->cpu_no);
    struct transport *tp = sx->io.tp;

    BUG_ON(!tp);

    /* Try flush buf massage into network before close */
    xio_connector_snd(sx);

    /* Detach xsock low-level file descriptor from poller */
    BUG_ON(eloop_del(&cpu->el, &sx->io.et) != 0);
    tp->close(sx->io.fd);

    sx->io.fd = -1;
    sx->io.et.events = -1;
    sx->io.et.fd = -1;
    sx->io.et.f = 0;
    sx->io.et.data = 0;
    sx->io.tp = 0;

    /* Destroy the xsock base and free xsockid. */
    xsock_free(sx);
}


static void rcv_head_notify(int xd, uint32_t events) {
    if (events & XMQ_POP)
	rcv_head_pop(xd);
    if (events & XMQ_FULL)
	rcv_head_full(xd);
    else if (events & XMQ_NONFULL)
	rcv_head_nonfull(xd);
}

static void snd_head_notify(int xd, uint32_t events) {
    if (events & XMQ_EMPTY)
	snd_head_empty(xd);
    else if (events & XMQ_NONEMPTY)
	snd_head_nonempty(xd);
}

static int msg_ready(struct bio *b, i64 *chunk_sz) {
    struct xmsg msg = {};
    
    if (b->bsize < sizeof(msg.vec))
	return false;
    bio_copy(b, (char *)(&msg.vec), sizeof(msg.vec));
    if (b->bsize < xiov_len(msg.vec.chunk))
	return false;
    *chunk_sz = msg.vec.size;
    return true;
}

static int xio_connector_rcv(struct xsock *sx) {
    int rc = 0;
    char *chunk;
    i64 chunk_sz;
    struct xmsg *msg;

    rc = bio_prefetch(&sx->io.in, &sx->io.ops);
    if (rc < 0 && errno != EAGAIN)
	return rc;
    while (msg_ready(&sx->io.in, &chunk_sz)) {
	DEBUG_OFF("%d xsock recv one message", sx->xd);
	chunk = xallocmsg(chunk_sz);
	bio_read(&sx->io.in, xiov_base(chunk), xiov_len(chunk));
	msg = cont_of(chunk, struct xmsg, vec.chunk);
	push_rcv(sx, msg);
    }
    return rc;
}

static int xio_connector_snd(struct xsock *sx) {
    int rc;
    char *chunk;
    struct xmsg *msg;

    while ((msg = pop_snd(sx))) {
	chunk = msg->vec.chunk;
	bio_write(&sx->io.out, xiov_base(chunk), xiov_len(chunk));
	xfreemsg(chunk);
    }
    rc = bio_flush(&sx->io.out, &sx->io.ops);
    return rc;
}

int xio_connector_handler(eloop_t *el, ev_t *et) {
    int rc = 0;
    struct xsock *sx = cont_of(et, struct xsock, io.et);

    if (et->happened & EPOLLIN) {
	DEBUG_OFF("io xsock %d EPOLLIN", sx->xd);
	rc = xio_connector_rcv(sx);
    }
    if (et->happened & EPOLLOUT) {
	DEBUG_OFF("io xsock %d EPOLLOUT", sx->xd);
	rc = xio_connector_snd(sx);
    }
    if ((rc < 0 && errno != EAGAIN) || et->happened & (EPOLLERR|EPOLLRDHUP)) {
	DEBUG_OFF("io xsock %d EPIPE", sx->xd);
	sx->fok = false;
    }

    /* Check events for xpoll */
    xpoll_notify(sx, 0);
    return rc;
}



struct xsock_protocol xtcp_connector_protocol = {
    .type = XCONNECTOR,
    .pf = PF_NET,
    .init = xio_connector_init,
    .destroy = xio_connector_destroy,
    .rcv_notify = rcv_head_notify,
    .snd_notify = snd_head_notify,
};

struct xsock_protocol xipc_connector_protocol = {
    .type = XCONNECTOR,
    .pf = PF_IPC,
    .init = xio_connector_init,
    .destroy = xio_connector_destroy,
    .rcv_notify = rcv_head_notify,
    .snd_notify = snd_head_notify,
};
