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
    struct xsock *self = cont_of(ops, struct xsock, io.ops);
    struct transport *tp = self->io.tp;

    BUG_ON(!tp);
    int rc = tp->read(self->io.sys_fd, buff, sz);
    return rc;
}

static i64 xio_connector_write(struct io *ops, char *buff, i64 sz) {
    struct xsock *self = cont_of(ops, struct xsock, io.ops);
    struct transport *tp = self->io.tp;

    BUG_ON(!tp);
    int rc = tp->write(self->io.sys_fd, buff, sz);
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
    struct xsock *self = xget(fd);
    struct xcpu *cpu = xcpuget(self->cpu_no);

    // Disable POLLOUT event when snd_head is empty
    if (self->io.et.events & EPOLLOUT) {
	DEBUG_OFF("%d disable EPOLLOUT", fd);
	self->io.et.events &= ~EPOLLOUT;
	BUG_ON(eloop_mod(&cpu->el, &self->io.et) != 0);
    }
}

static void snd_head_nonempty(int fd) {
    struct xsock *self = xget(fd);
    struct xcpu *cpu = xcpuget(self->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if (!(self->io.et.events & EPOLLOUT)) {
	DEBUG_OFF("%d enable EPOLLOUT", fd);
	self->io.et.events |= EPOLLOUT;
	BUG_ON(eloop_mod(&cpu->el, &self->io.et) != 0);
    }
}


/******************************************************************************
 *  rcv_head events trigger.
 ******************************************************************************/

static void rcv_head_pop(int fd) {
    struct xsock *self = xget(fd);

    if (self->snd.waiters)
	condition_signal(&self->cond);
}

static void rcv_head_full(int fd) {
    struct xsock *self = xget(fd);    
    struct xcpu *cpu = xcpuget(self->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if ((self->io.et.events & EPOLLIN)) {
	self->io.et.events &= ~EPOLLIN;
	BUG_ON(eloop_mod(&cpu->el, &self->io.et) != 0);
    }
}

static void rcv_head_nonfull(int fd) {
    struct xsock *self = xget(fd);    
    struct xcpu *cpu = xcpuget(self->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if (!(self->io.et.events & EPOLLIN)) {
	self->io.et.events |= EPOLLIN;
	BUG_ON(eloop_mod(&cpu->el, &self->io.et) != 0);
    }
}

int xio_connector_handler(eloop_t *el, ev_t *et);

void xio_connector_init(struct xsock *self,
			struct transport *tp, int s) {
    int on = 1;
    struct xcpu *cpu = xcpuget(self->cpu_no);

    ZERO(self->io);
    bio_init(&self->io.in);
    bio_init(&self->io.out);

    tp->setopt(s, TP_NOBLOCK, &on, sizeof(on));
    self->io.sys_fd = s;
    self->io.tp = tp;
    self->io.ops = default_xops;
    self->io.et.events = EPOLLIN|EPOLLRDHUP|EPOLLERR;
    self->io.et.fd = s;
    self->io.et.f = xio_connector_handler;
    self->io.et.data = self;
    BUG_ON(eloop_add(&cpu->el, &self->io.et) != 0);
}

static int xio_connector_bind(int fd, const char *sock) {
    int s;
    struct xsock *self = xget(fd);
    struct transport *tp = transport_lookup(self->pf);

    BUG_ON(!tp);
    if ((s = tp->connect(sock)) < 0)
	return -1;
    strncpy(self->peer, sock, TP_SOCKADDRLEN);
    xio_connector_init(self, tp, s);
    return 0;
}

static int xio_connector_snd(struct xsock *self);

static void xio_connector_close(int fd) {
    struct xsock *self = xget(fd);
    struct xcpu *cpu = xcpuget(self->cpu_no);
    struct transport *tp = self->io.tp;

    BUG_ON(!tp);

    /* Try flush buf massage into network before close */
    xio_connector_snd(self);

    /* Detach xsock low-level file descriptor from poller */
    BUG_ON(eloop_del(&cpu->el, &self->io.et) != 0);
    tp->close(self->io.sys_fd);

    self->io.sys_fd = -1;
    self->io.et.events = -1;
    self->io.et.fd = -1;
    self->io.et.f = 0;
    self->io.et.data = 0;
    self->io.tp = 0;

    /* Destroy the xsock base and free xsockid. */
    xsock_free(self);
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

static int bufio_check_msg(struct bio *b) {
    struct xmsg aim = {};
    
    if (b->bsize < sizeof(aim.vec))
	return false;
    bio_copy(b, (char *)(&aim.vec), sizeof(aim.vec));
    if (b->bsize < xiov_len(aim.vec.chunk) + (u32)aim.vec.oob_length)
	return false;
    return true;
}


static void bufio_rm(struct bio *b, struct xmsg **msg) {
    struct xmsg one = {};
    char *chunk;

    bio_copy(b, (char *)(&one.vec), sizeof(one.vec));
    chunk = xallocmsg(one.vec.size);
    bio_read(b, xiov_base(chunk), xiov_len(chunk));
    *msg = cont_of(chunk, struct xmsg, vec.chunk);
}

static int xio_connector_rcv(struct xsock *self) {
    int rc = 0;
    u16 oob_count;
    struct xmsg *aim = 0, *oob = 0;

    rc = bio_prefetch(&self->io.in, &self->io.ops);
    if (rc < 0 && errno != EAGAIN)
	return rc;
    while (bufio_check_msg(&self->io.in)) {
	aim = 0;
	bufio_rm(&self->io.in, &aim);
	BUG_ON(!aim);
	oob_count = aim->vec.oob;
	while (oob_count--) {
	    oob = 0;
	    bufio_rm(&self->io.in, &oob);
	    BUG_ON(!oob);
	    list_add_tail(&oob->item, &aim->oob);
	}
	recvq_push(self, aim);
	DEBUG_OFF("%d xsock recv one message", self->fd);
    }
    return rc;
}

static void bufio_add(struct bio *b, struct xmsg *msg) {
    struct xmsg *oob, *nx_oob;
    char *chunk = msg->vec.chunk;

    bio_write(b, xiov_base(chunk), xiov_len(chunk));
    xmsg_walk_safe(oob, nx_oob, &msg->oob) {
	chunk = oob->vec.chunk;
	bio_write(b, xiov_base(chunk), xiov_len(chunk));
    }
}

static int xio_connector_snd(struct xsock *self) {
    int rc;
    struct xmsg *msg;

    while ((msg = sendq_pop(self))) {
	bufio_add(&self->io.out, msg);
	xfreemsg(msg->vec.chunk);
    }
    rc = bio_flush(&self->io.out, &self->io.ops);
    return rc;
}

int xio_connector_handler(eloop_t *el, ev_t *et) {
    int rc = 0;
    struct xsock *self = cont_of(et, struct xsock, io.et);

    if (et->happened & EPOLLIN) {
	DEBUG_OFF("io xsock %d EPOLLIN", self->fd);
	rc = xio_connector_rcv(self);
    }
    if (et->happened & EPOLLOUT) {
	DEBUG_OFF("io xsock %d EPOLLOUT", self->fd);
	rc = xio_connector_snd(self);
    }
    if ((rc < 0 && errno != EAGAIN) || et->happened & (EPOLLERR|EPOLLRDHUP)) {
	DEBUG_OFF("io xsock %d EPIPE", self->fd);
	self->fok = false;
    }
    xeventnotify(self);
    return rc;
}



struct sockbase_vfptr xtcp_connector_spec = {
    .type = XCONNECTOR,
    .pf = XPF_TCP,
    .bind = xio_connector_bind,
    .close = xio_connector_close,
    .notify = xio_connector_notify,
    .setopt = 0,
    .getopt = 0,
};

struct sockbase_vfptr xipc_connector_spec = {
    .type = XCONNECTOR,
    .pf = XPF_IPC,
    .bind = xio_connector_bind,
    .close = xio_connector_close,
    .notify = xio_connector_notify,
    .setopt = 0,
    .getopt = 0,
};
