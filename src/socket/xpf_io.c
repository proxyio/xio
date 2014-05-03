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
    struct xsock *xsk = cont_of(ops, struct xsock, io.ops);
    struct transport *tp = xsk->io.tp;

    BUG_ON(!tp);
    int rc = tp->read(xsk->io.sys_fd, buff, sz);
    return rc;
}

static i64 xio_connector_write(struct io *ops, char *buff, i64 sz) {
    struct xsock *xsk = cont_of(ops, struct xsock, io.ops);
    struct transport *tp = xsk->io.tp;

    BUG_ON(!tp);
    int rc = tp->write(xsk->io.sys_fd, buff, sz);
    return rc;
}

struct io default_xops = {
    .read = xio_connector_read,
    .write = xio_connector_write,
};



/******************************************************************************
 *  snd_head events trigger.
 ******************************************************************************/

static void snd_head_empty(int fd) {
    struct xsock *xsk = xget(fd);
    struct xcpu *cpu = xcpuget(xsk->cpu_no);

    // Disable POLLOUT event when snd_head is empty
    if (xsk->io.et.events & EPOLLOUT) {
	DEBUG_OFF("%d disable EPOLLOUT", fd);
	xsk->io.et.events &= ~EPOLLOUT;
	BUG_ON(eloop_mod(&cpu->el, &xsk->io.et) != 0);
    }
}

static void snd_head_nonempty(int fd) {
    struct xsock *xsk = xget(fd);
    struct xcpu *cpu = xcpuget(xsk->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if (!(xsk->io.et.events & EPOLLOUT)) {
	DEBUG_OFF("%d enable EPOLLOUT", fd);
	xsk->io.et.events |= EPOLLOUT;
	BUG_ON(eloop_mod(&cpu->el, &xsk->io.et) != 0);
    }
}


/******************************************************************************
 *  rcv_head events trigger.
 ******************************************************************************/

static void rcv_head_pop(int fd) {
    struct xsock *xsk = xget(fd);

    if (xsk->snd_waiters)
	condition_signal(&xsk->cond);
}

static void rcv_head_full(int fd) {
    struct xsock *xsk = xget(fd);    
    struct xcpu *cpu = xcpuget(xsk->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if ((xsk->io.et.events & EPOLLIN)) {
	xsk->io.et.events &= ~EPOLLIN;
	BUG_ON(eloop_mod(&cpu->el, &xsk->io.et) != 0);
    }
}

static void rcv_head_nonfull(int fd) {
    struct xsock *xsk = xget(fd);    
    struct xcpu *cpu = xcpuget(xsk->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if (!(xsk->io.et.events & EPOLLIN)) {
	xsk->io.et.events |= EPOLLIN;
	BUG_ON(eloop_mod(&cpu->el, &xsk->io.et) != 0);
    }
}

int xio_connector_handler(eloop_t *el, ev_t *et);

void xio_connector_init(struct xsock *xsk,
			struct transport *tp, int s) {
    int on = 1;
    struct xcpu *cpu = xcpuget(xsk->cpu_no);

    ZERO(xsk->io);
    bio_init(&xsk->io.in);
    bio_init(&xsk->io.out);

    tp->setopt(s, TP_NOBLOCK, &on, sizeof(on));
    xsk->io.sys_fd = s;
    xsk->io.tp = tp;
    xsk->io.ops = default_xops;
    xsk->io.et.events = EPOLLIN|EPOLLRDHUP|EPOLLERR;
    xsk->io.et.fd = s;
    xsk->io.et.f = xio_connector_handler;
    xsk->io.et.data = xsk;
    BUG_ON(eloop_add(&cpu->el, &xsk->io.et) != 0);
}

static int xio_connector_bind(int fd, const char *sock) {
    int s;
    struct xsock *xsk = xget(fd);
    struct transport *tp = transport_lookup(xsk->pf);

    BUG_ON(!tp);
    if ((s = tp->connect(sock)) < 0)
	return -1;
    strncpy(xsk->peer, sock, TP_SOCKADDRLEN);
    xio_connector_init(xsk, tp, s);
    return 0;
}

static int xio_connector_snd(struct xsock *xsk);

static void xio_connector_close(int fd) {
    struct xsock *xsk = xget(fd);
    struct xcpu *cpu = xcpuget(xsk->cpu_no);
    struct transport *tp = xsk->io.tp;

    BUG_ON(!tp);

    /* Try flush buf massage into network before close */
    xio_connector_snd(xsk);

    /* Detach xsock low-level file descriptor from poller */
    BUG_ON(eloop_del(&cpu->el, &xsk->io.et) != 0);
    tp->close(xsk->io.sys_fd);

    xsk->io.sys_fd = -1;
    xsk->io.et.events = -1;
    xsk->io.et.fd = -1;
    xsk->io.et.f = 0;
    xsk->io.et.data = 0;
    xsk->io.tp = 0;

    /* Destroy the xsock base and free xsockid. */
    xsock_free(xsk);
}


static void rcv_head_notify(int fd, u32 events) {
    if (events & XMQ_POP)
	rcv_head_pop(fd);
    if (events & XMQ_FULL)
	rcv_head_full(fd);
    else if (events & XMQ_NONFULL)
	rcv_head_nonfull(fd);
}

static void snd_head_notify(int fd, u32 events) {
    if (events & XMQ_EMPTY)
	snd_head_empty(fd);
    else if (events & XMQ_NONEMPTY)
	snd_head_nonempty(fd);
}

static void xio_connector_notify(int fd, int type, u32 events) {
    switch (type) {
    case RECV_Q:
	rcv_head_notify(fd, events);
	break;
    case SEND_Q:
	snd_head_notify(fd, events);
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

static int xio_connector_rcv(struct xsock *xsk) {
    int rc = 0;
    char *chunk;
    i64 chunk_sz;
    struct xmsg *msg;

    rc = bio_prefetch(&xsk->io.in, &xsk->io.ops);
    if (rc < 0 && errno != EAGAIN)
	return rc;
    while (msg_ready(&xsk->io.in, &chunk_sz)) {
	DEBUG_OFF("%d xsock recv one message", xsk->fd);
	chunk = xallocmsg(chunk_sz);
	bio_read(&xsk->io.in, xiov_base(chunk), xiov_len(chunk));
	msg = cont_of(chunk, struct xmsg, vec.chunk);
	recvq_push(xsk, msg);
    }
    return rc;
}

static int xio_connector_snd(struct xsock *xsk) {
    int rc;
    char *chunk;
    struct xmsg *msg;

    while ((msg = sendq_pop(xsk))) {
	chunk = msg->vec.chunk;
	bio_write(&xsk->io.out, xiov_base(chunk), xiov_len(chunk));
	xfreemsg(chunk);
    }
    rc = bio_flush(&xsk->io.out, &xsk->io.ops);
    return rc;
}

int xio_connector_handler(eloop_t *el, ev_t *et) {
    int rc = 0;
    struct xsock *xsk = cont_of(et, struct xsock, io.et);

    if (et->happened & EPOLLIN) {
	DEBUG_OFF("io xsock %d EPOLLIN", xsk->fd);
	rc = xio_connector_rcv(xsk);
    }
    if (et->happened & EPOLLOUT) {
	DEBUG_OFF("io xsock %d EPOLLOUT", xsk->fd);
	rc = xio_connector_snd(xsk);
    }
    if ((rc < 0 && errno != EAGAIN) || et->happened & (EPOLLERR|EPOLLRDHUP)) {
	DEBUG_OFF("io xsock %d EPIPE", xsk->fd);
	xsk->fok = false;
    }
    xpoll_notify(xsk);
    return rc;
}



struct xsock_protocol xtcp_connector_protocol = {
    .type = XCONNECTOR,
    .pf = XPF_TCP,
    .bind = xio_connector_bind,
    .close = xio_connector_close,
    .notify = xio_connector_notify,
    .setsockopt = 0,
    .getsockopt = 0,
};

struct xsock_protocol xipc_connector_protocol = {
    .type = XCONNECTOR,
    .pf = XPF_IPC,
    .bind = xio_connector_bind,
    .close = xio_connector_close,
    .notify = xio_connector_notify,
    .setsockopt = 0,
    .getsockopt = 0,
};
