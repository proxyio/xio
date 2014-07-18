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
#include <rex/rex.h>
#include "tcp.h"

extern struct io default_xops;

static void tcp_listener_hndl (struct ev_fdset *evfds, struct ev_fd *evfd, int events);

static int tcp_listener_bind (struct sockbase *sb, const char *sock)
{
	struct ev_loop *evl;
	struct tcp_sock *tcps;
	int rc;
	int on = 1;

	evl = sb->evl;
	tcps = cont_of (sb, struct tcp_sock, base);


	if ((rc = rex_sock_listen (&tcps->s, sock)) < 0)
		return -1;
	strcpy (sb->addr, sock);
	rex_sock_setopt (&tcps->s, REX_SO_NOBLOCK, &on, sizeof (on));
	tcps->et.events = EV_READ;
	tcps->et.fd = tcps->s.ss_fd;
	tcps->et.hndl = tcp_listener_hndl;
	BUG_ON (ev_fdset_ctl (&evl->fdset, EV_ADD, &tcps->et) != 0);
	return 0;
}

static void tcp_listener_close (struct sockbase *sb)
{
	struct ev_loop *evl;
	struct tcp_sock *tcps;
	struct sockbase *tmp;

	evl = sb->evl;
	tcps = cont_of (sb, struct tcp_sock, base);

	/* Detach sock low-level file descriptor from poller */
	BUG_ON (ev_fdset_ctl (&evl->fdset, EV_DEL, &tcps->et) != 0);
	rex_sock_destroy (&tcps->s);

	/* Destroy acceptq's connection */
	while (acceptq_rm_nohup (sb, &tmp) == 0)
		__xclose (tmp);

	/* Destroy the sock base and free sockid. */
	sockbase_exit (sb);
	mem_free (tcps, sizeof (*tcps));
}

extern void tcp_socket_init (struct tcp_sock *sk);

static void tcp_listener_hndl (struct ev_fdset *evfds, struct ev_fd *evfd, int events)
{
	int rc;
	int nfd;
	struct tcp_sock *tcps;
	struct sockbase *sb;
	struct tcp_sock *ntcps;
	struct sockbase *nsb;

	tcps = cont_of (evfd, struct tcp_sock, et);
	sb = &tcps->base;
	nfd = xalloc (sb->vfptr->pf, XCONNECTOR);
	nsb = xgb.sockbases[nfd];
	ntcps = cont_of (nsb, struct tcp_sock, base);

	if ((rc = rex_sock_accept (&tcps->s, &ntcps->s)) < 0) {
		if (errno != EAGAIN) {
			mutex_lock (&sb->lock);
			sb->flagset.epipe = true;
			if (sb->acceptq.waiters)
				condition_broadcast (&sb->acceptq.cond);
			mutex_unlock (&sb->lock);
		}
		__xclose (xgb.sockbases[nfd]);
		return;
	}
	SKLOG_DEBUG (sb, "%d accept new connection %d", sb->fd, nfd);
	tcp_socket_init (ntcps);
	__ev_fdset_ctl (&nsb->evl->fdset, EV_ADD, &ntcps->et);
	acceptq_add (sb, nsb);
}

extern struct sockbase *tcp_alloc();
extern struct sockbase *ipc_alloc();

struct sockbase_vfptr tcp_listener_vfptr = {
	.type = XLISTENER,
	.pf = TP_TCP,
	.alloc = tcp_alloc,
	.send = 0,
	.bind = tcp_listener_bind,
	.close = tcp_listener_close,
	.getopt = 0,
	.setopt = 0,
};

struct sockbase_vfptr ipc_listener_vfptr = {
	.type = XLISTENER,
	.pf = TP_IPC,
	.alloc = ipc_alloc,
	.send = 0,
	.bind = tcp_listener_bind,
	.close = tcp_listener_close,
	.getopt = 0,
	.setopt = 0,
};
