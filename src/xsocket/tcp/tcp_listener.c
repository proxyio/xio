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
#include <utils/log.h>
#include "../xg.h"

extern struct io default_xops;

static void tcp_listener_hndl (struct ev_fdset *evfds, struct ev_fd *evfd, int events);

static int tcp_listener_bind (struct sockbase *sb, const char *sock)
{
	struct ev_loop *evl;
	struct tcp_sock *tcpsk;
	int on = 1;
	int sys_fd;

	evl = sb->evl;
	tcpsk = cont_of (sb, struct tcp_sock, base);
	
	tcpsk->vtp = tp_get (sb->vfptr->pf);
	if ((sys_fd = tcpsk->vtp->bind (sock)) < 0)
		return -1;
	strcpy (sb->addr, sock);
	tcpsk->vtp->setopt (sys_fd, TP_NOBLOCK, &on, sizeof (on));

	tcpsk->sys_fd = sys_fd;
	tcpsk->et.events = EV_READ;
	tcpsk->et.fd = sys_fd;
	tcpsk->et.hndl = tcp_listener_hndl;
	BUG_ON (ev_fdset_ctl (&evl->fdset, EV_ADD, &tcpsk->et) != 0);
	return 0;
}

static void tcp_listener_close (struct sockbase *sb)
{
	struct ev_loop *evl;
	struct tcp_sock *tcpsk;
	struct sockbase *tmp;

	evl = sb->evl;
	tcpsk = cont_of (sb, struct tcp_sock, base);

	/* Detach sock low-level file descriptor from poller */
	BUG_ON (ev_fdset_ctl (&evl->fdset, EV_DEL, &tcpsk->et) != 0);
	tcpsk->vtp->close (tcpsk->sys_fd);

	/* Destroy acceptq's connection */
	while (acceptq_rm_nohup (sb, &tmp) == 0)
		xclose (tmp->fd);

	/* Destroy the sock base and free sockid. */
	sockbase_exit (sb);
	mem_free (tcpsk, sizeof (*tcpsk));
}

extern int tcp_socket_init (struct sockbase *sb, int sys_fd);

static void tcp_listener_hndl (struct ev_fdset *evfds, struct ev_fd *evfd, int events)
{
	struct tcp_sock *tcpsk = cont_of (evfd, struct tcp_sock, et);
	struct sockbase *sb = &tcpsk->base;
	int sys_fd;
	int fd_new;
	struct sockbase *sb_new = 0;

	if ((sys_fd = tcpsk->vtp->accept (tcpsk->sys_fd)) < 0) {
		if (errno != EAGAIN) {
			mutex_lock (&sb->lock);
			sb->flagset.epipe = true;
			if (sb->acceptq.waiters)
				condition_broadcast (&sb->acceptq.cond);
			mutex_unlock (&sb->lock);
		}
		return;
	}
	if ((fd_new = xalloc (sb->vfptr->pf, XCONNECTOR)) < 0) {
		tcpsk->vtp->close (sys_fd);
		return;
	}
	sb_new = xgb.sockbases[fd_new];
	LOG_DEBUG (dlv (sb), "%d accept new connection %d", sb->fd, fd_new);

	BUG_ON (tcp_socket_init (sb_new, sys_fd));
	acceptq_add (sb, sb_new);
}

extern struct sockbase *tcp_alloc();

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
	.alloc = tcp_alloc,
	.send = 0,
	.bind = tcp_listener_bind,
	.close = tcp_listener_close,
	.getopt = 0,
	.setopt = 0,
};
