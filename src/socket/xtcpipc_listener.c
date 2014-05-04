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

extern struct io default_xops;

/***************************************************************************
 *  request_socks events trigger.
 ***************************************************************************/

static void request_socks_full(int fd) {
    struct xsock *self = xget(fd);    
    struct xcpu *cpu = xcpuget(self->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if ((self->io.et.events & EPOLLIN)) {
	self->io.et.events &= ~EPOLLIN;
	BUG_ON(eloop_mod(&cpu->el, &self->io.et) != 0);
    }
}

static void request_socks_nonfull(int fd) {
    struct xsock *self = xget(fd);    
    struct xcpu *cpu = xcpuget(self->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if (!(self->io.et.events & EPOLLIN)) {
	self->io.et.events |= EPOLLIN;
	BUG_ON(eloop_mod(&cpu->el, &self->io.et) != 0);
    }
}

static int xio_listener_handler(eloop_t *el, ev_t *et);

static int xio_listener_bind(int fd, const char *sock) {
    int s, on = 1;
    struct xsock *self = xget(fd);
    struct xcpu *cpu = xcpuget(self->cpu_no);
    struct transport *tp = transport_lookup(self->pf);

    BUG_ON(!tp);
    if ((s = tp->bind(sock)) < 0)
	return -1;

    ZERO(self->io);
    strncpy(self->addr, sock, TP_SOCKADDRLEN);

    tp->setopt(s, TP_NOBLOCK, &on, sizeof(on));
    self->io.sys_fd = s;
    self->io.tp = tp;
    self->io.et.events = EPOLLIN|EPOLLERR;
    self->io.et.fd = s;
    self->io.et.f = xio_listener_handler;
    self->io.et.data = self;

    BUG_ON(eloop_add(&cpu->el, &self->io.et) != 0);
    return 0;
}

static void xio_listener_close(int fd) {
    struct xsock *self = xget(fd);
    struct xcpu *cpu = xcpuget(self->cpu_no);
    struct transport *tp = self->io.tp;

    BUG_ON(!tp);

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

static void request_socks_notify(int fd, u32 events) {
    if (events & XMQ_FULL)
	request_socks_full(fd);
    else if (events & XMQ_NONFULL)
	request_socks_nonfull(fd);
}

static void xio_listener_notify(int fd, int type, uint32_t events) {
    switch (type) {
    case SOCKS_REQ:
	request_socks_notify(fd, events);
	break;
    default:
	BUG_ON(1);
    }
}

extern int xio_connector_handler(eloop_t *el, ev_t *et);
extern void xio_connector_init(struct xsock *self,
			       struct transport *tp, int s);

static int xio_listener_handler(eloop_t *el, ev_t *et) {
    int s;
    struct xsock *self = cont_of(et, struct xsock, io.et);
    struct transport *tp = self->io.tp;
    struct xsock *new;

    if ((et->happened & EPOLLERR) || !(et->happened & EPOLLIN)) {
	self->fok = false;
	errno = EPIPE;
	return -1;
    } 
    if ((s = tp->accept(self->io.sys_fd)) < 0)
	return -1;
    if (!(new = xsock_alloc())) {
	errno = EMFILE;
	return -1;
    }
    DEBUG_OFF("xsock accept new connection %d", s);
    new->type = XCONNECTOR;
    new->pf = self->pf;
    new->sockspec_vfptr = sockspec_lookup(new->pf, new->type);
    xio_connector_init(new, tp, s);
    acceptq_push(self, new);
    return 0;
}

struct sockspec xtcp_listener_spec = {
    .type = XLISTENER,
    .pf = XPF_TCP,
    .bind = xio_listener_bind,
    .close = xio_listener_close,
    .notify = xio_listener_notify,
    .getsockopt = 0,
    .setsockopt = 0,
};

struct sockspec xipc_listener_spec = {
    .type = XLISTENER,
    .pf = XPF_IPC,
    .bind = xio_listener_bind,
    .close = xio_listener_close,
    .notify = xio_listener_notify,
    .getsockopt = 0,
    .setsockopt = 0,
};
