/*
  Copyright (c) 2013-2014 Dong Fang. All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <runner/taskpool.h>
#include "xgb.h"

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
	DEBUG_OFF("%d disable EPOLLOUT", xd);
	sx->io.et.events &= ~EPOLLOUT;
	BUG_ON(eloop_mod(&cpu->el, &sx->io.et) != 0);
    }
}

static void snd_head_nonempty(int xd) {
    struct xsock *sx = xget(xd);
    struct xcpu *cpu = xcpuget(sx->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if (!(sx->io.et.events & EPOLLOUT)) {
	DEBUG_OFF("%d enable EPOLLOUT", xd);
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

void xio_connector_init(struct xsock *sx,
			struct transport *tp, int s) {
    int on = 1;
    struct xcpu *cpu = xcpuget(sx->cpu_no);

    ZERO(sx->io);
    bio_init(&sx->io.in);
    bio_init(&sx->io.out);

    tp->setopt(s, TP_NOBLOCK, &on, sizeof(on));
    sx->io.fd = s;
    sx->io.tp = tp;
    sx->io.ops = default_xops;
    sx->io.et.events = EPOLLIN|EPOLLRDHUP|EPOLLERR;
    sx->io.et.fd = s;
    sx->io.et.f = xio_connector_handler;
    sx->io.et.data = sx;
    BUG_ON(eloop_add(&cpu->el, &sx->io.et) != 0);
}

static int xio_connector_bind(int xd, const char *sock) {
    int s;
    struct xsock *sx = xget(xd);
    struct transport *tp = transport_lookup(sx->pf);

    BUG_ON(!tp);
    if ((s = tp->connect(sock)) < 0)
	return -1;
    strncpy(sx->peer, sock, TP_SOCKADDRLEN);
    xio_connector_init(sx, tp, s);
    return 0;
}

static int xio_connector_snd(struct xsock *sx);

static void xio_connector_close(int xd) {
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


static void rcv_head_notify(int xd, u32 events) {
    if (events & XMQ_POP)
	rcv_head_pop(xd);
    if (events & XMQ_FULL)
	rcv_head_full(xd);
    else if (events & XMQ_NONFULL)
	rcv_head_nonfull(xd);
}

static void snd_head_notify(int xd, u32 events) {
    if (events & XMQ_EMPTY)
	snd_head_empty(xd);
    else if (events & XMQ_NONEMPTY)
	snd_head_nonempty(xd);
}

static void xio_connector_notify(int xd, int type, u32 events) {
    switch (type) {
    case RECV_Q:
	rcv_head_notify(xd, events);
	break;
    case SEND_Q:
	snd_head_notify(xd, events);
	break;
    default:
	BUG_ON(1);
    }
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
	recvq_push(sx, msg);
    }
    return rc;
}

static int xio_connector_snd(struct xsock *sx) {
    int rc;
    char *chunk;
    struct xmsg *msg;

    while ((msg = sendq_pop(sx))) {
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
    xpoll_notify(sx);
    return rc;
}



struct xsock_protocol xtcp_connector_protocol = {
    .type = XCONNECTOR,
    .pf = XPF_TCP,
    .bind = xio_connector_bind,
    .close = xio_connector_close,
    .notify = xio_connector_notify,
};

struct xsock_protocol xipc_connector_protocol = {
    .type = XCONNECTOR,
    .pf = XPF_IPC,
    .bind = xio_connector_bind,
    .close = xio_connector_close,
    .notify = xio_connector_notify,
};
