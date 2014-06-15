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
#include <utils/taskpool.h>
#include "../xgb.h"

extern struct io default_xops;

/***************************************************************************
 *  request_socks events trigger.
 ***************************************************************************/

static void request_socks_full (struct sockbase *sb)
{
	struct tcpipc_sock *self = cont_of (sb, struct tcpipc_sock, base);
	struct worker *cpu = get_worker (sb->cpu_no);

	// Enable POLLOUT event when snd_head isn't empty
	if ( (self->et.events & EPOLLIN) ) {
		self->et.events &= ~EPOLLIN;
		BUG_ON (eloop_mod (&cpu->el, &self->et) != 0);
	}
}

static void request_socks_nonfull (struct sockbase *sb)
{
	struct tcpipc_sock *self = cont_of (sb, struct tcpipc_sock, base);
	struct worker *cpu = get_worker (sb->cpu_no);

	// Enable POLLOUT event when snd_head isn't empty
	if (! (self->et.events & EPOLLIN) ) {
		self->et.events |= EPOLLIN;
		BUG_ON (eloop_mod (&cpu->el, &self->et) != 0);
	}
}

static int ti_listener_hndl (eloop_t *el, ev_t *et);

static int ti_listener_bind (struct sockbase *sb, const char *sock)
{
	struct tcpipc_sock *self = cont_of (sb, struct tcpipc_sock, base);
	int sys_fd, on = 1;
	struct worker *cpu = get_worker (sb->cpu_no);

	BUG_ON (! (self->vtp = transport_lookup (sb->vfptr->pf) ) );
	if ( (sys_fd = self->vtp->bind (sock) ) < 0)
		return -1;
	strncpy (sb->addr, sock, TP_SOCKADDRLEN);
	self->vtp->setopt (sys_fd, TP_NOBLOCK, &on, sizeof (on) );

	self->sys_fd = sys_fd;
	self->et.events = EPOLLIN|EPOLLERR;
	self->et.fd = sys_fd;
	self->et.f = ti_listener_hndl;
	self->et.data = self;
	BUG_ON (eloop_add (&cpu->el, &self->et) != 0);
	return 0;
}

static void ti_listener_close (struct sockbase *sb)
{
	struct tcpipc_sock *self = cont_of (sb, struct tcpipc_sock, base);
	struct worker *cpu = get_worker (sb->cpu_no);
	struct sockbase *nsb;

	BUG_ON (!self->vtp);

	/* Detach sock low-level file descriptor from poller */
	BUG_ON (eloop_del (&cpu->el, &self->et) != 0);
	self->vtp->close (self->sys_fd);

	self->sys_fd = -1;
	self->et.events = -1;
	self->et.fd = -1;
	self->et.f = 0;
	self->et.data = 0;
	self->vtp = 0;

	/* Destroy acceptq's connection */
	while (acceptq_rm_nohup (sb, &nsb) == 0) {
		xclose (nsb->fd);
	}

	/* Destroy the sock base and free sockid. */
	sockbase_exit (sb);
	mem_free (self, sizeof (*self) );
}

static void request_socks_notify (struct sockbase *sb, u32 events)
{
	if (events & XMQ_FULL)
		request_socks_full (sb);
	else if (events & XMQ_NONFULL)
		request_socks_nonfull (sb);
}

static void ti_listener_notify (struct sockbase *sb, int type, uint32_t events)
{
	switch (type) {
	case SOCKS_REQ:
		request_socks_notify (sb, events);
		break;
	default:
		BUG_ON (1);
	}
}

extern int ti_connector_hndl (eloop_t *el, ev_t *et);

static int ti_listener_hndl (eloop_t *el, ev_t *et)
{
	struct tcpipc_sock *self = cont_of (et, struct tcpipc_sock, et);
	int sys_fd;
	struct sockbase *sb = &self->base;
	int on = 1;
	int nfd;
	struct worker *cpu;
	struct sockbase *nsb = 0;
	struct tcpipc_sock *nself = 0;

	if ( (et->happened & EPOLLERR) && ! (et->happened & EPOLLIN) ) {
		mutex_lock (&sb->lock);
		sb->fepipe = true;
		if (sb->acceptq.waiters)
			condition_broadcast (&sb->acceptq.cond);
		mutex_unlock (&sb->lock);
		return -1;
	}
	BUG_ON (!self->vtp);
	if ( (sys_fd = self->vtp->accept (self->sys_fd) ) < 0) {
		return -1;
	}
	if ( (nfd = xalloc (sb->vfptr->pf, XCONNECTOR) ) < 0) {
		self->vtp->close (sys_fd);
		return -1;
	}
	nsb = xgb.sockbases[nfd];
	cpu = get_worker (nsb->cpu_no);
	nself = cont_of (nsb, struct tcpipc_sock, base);

	DEBUG_OFF ("%d accept new connection %d", sb->fd, nfd);
	nself->sys_fd = sys_fd;
	nself->vtp = self->vtp;
	BUG_ON (nself->vtp->setopt (sys_fd, TP_NOBLOCK, &on, sizeof (on) ) );
	nself->ops = default_xops;
	nself->et.events = EPOLLIN|EPOLLRDHUP|EPOLLERR;
	nself->et.fd = sys_fd;
	nself->et.f = ti_connector_hndl;
	nself->et.data = nself;
	BUG_ON (eloop_add (&cpu->el, &nself->et) != 0);
	return acceptq_add (sb, nsb);
}

extern struct sockbase *ti_alloc();

struct sockbase_vfptr tcp_listener_spec = {
	.type = XLISTENER,
	.pf = XPF_TCP,
	.alloc = ti_alloc,
	.bind = ti_listener_bind,
	.close = ti_listener_close,
	.notify = ti_listener_notify,
	.getopt = 0,
	.setopt = 0,
};

struct sockbase_vfptr ipc_listener_spec = {
	.type = XLISTENER,
	.pf = XPF_IPC,
	.alloc = ti_alloc,
	.bind = ti_listener_bind,
	.close = ti_listener_close,
	.notify = ti_listener_notify,
	.getopt = 0,
	.setopt = 0,
};
