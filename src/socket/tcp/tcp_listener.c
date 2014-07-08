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
#include "../global.h"

extern struct io default_xops;

static int tcp_listener_hndl (eloop_t *el, ev_t *et);

static int tcp_listener_bind (struct sockbase *sb, const char *sock)
{
	struct tcp_sock *tcpsk = cont_of (sb, struct tcp_sock, base);
	int sys_fd, on = 1;
	struct worker *cpu = get_worker (sb->cpu_no);

	tcpsk->vtp = tp_get (sb->vfptr->pf);
	if ((sys_fd = tcpsk->vtp->bind (sock)) < 0)
		return -1;
	strcpy (sb->addr, sock);
	tcpsk->vtp->setopt (sys_fd, TP_NOBLOCK, &on, sizeof (on));

	tcpsk->sys_fd = sys_fd;
	tcpsk->et.events = EPOLLIN|EPOLLERR;
	tcpsk->et.fd = sys_fd;
	tcpsk->et.f = tcp_listener_hndl;
	tcpsk->et.data = tcpsk;
	BUG_ON (eloop_add (&cpu->el, &tcpsk->et) != 0);
	return 0;
}

static void tcp_listener_close (struct sockbase *sb)
{
	struct tcp_sock *tcpsk = cont_of (sb, struct tcp_sock, base);
	struct worker *cpu = get_worker (sb->cpu_no);
	struct sockbase *nsb;

	BUG_ON (!tcpsk->vtp);

	/* Detach sock low-level file descriptor from poller */
	BUG_ON (eloop_del (&cpu->el, &tcpsk->et) != 0);
	tcpsk->vtp->close (tcpsk->sys_fd);

	tcpsk->sys_fd = -1;
	tcpsk->et.events = -1;
	tcpsk->et.fd = -1;
	tcpsk->et.f = 0;
	tcpsk->et.data = 0;
	tcpsk->vtp = 0;

	/* Destroy acceptq's connection */
	while (acceptq_rm_nohup (sb, &nsb) == 0) {
		xclose (nsb->fd);
	}

	/* Destroy the sock base and free sockid. */
	sockbase_exit (sb);
	mem_free (tcpsk, sizeof (*tcpsk));
}

extern int tcp_socket_init (struct sockbase *sb, int sys_fd);

static int tcp_listener_hndl (eloop_t *el, ev_t *et)
{
	struct tcp_sock *tcpsk = cont_of (et, struct tcp_sock, et);
	struct sockbase *sb = &tcpsk->base;
	int sys_fd;
	int fd_new;
	struct sockbase *sb_new = 0;

	if ((et->happened & EPOLLERR) && !(et->happened & EPOLLIN)) {
		mutex_lock (&sb->lock);
		sb->fepipe = true;
		if (sb->acceptq.waiters)
			condition_broadcast (&sb->acceptq.cond);
		mutex_unlock (&sb->lock);
		return -1;
	}
	BUG_ON (!tcpsk->vtp);
	if ((sys_fd = tcpsk->vtp->accept (tcpsk->sys_fd)) < 0) {
		return -1;
	}
	if ((fd_new = xalloc (sb->vfptr->pf, XCONNECTOR)) < 0) {
		tcpsk->vtp->close (sys_fd);
		return -1;
	}
	sb_new = xgb.sockbases[fd_new];
	DEBUG_OFF ("%d accept new connection %d", sb->fd, fd_new);

	BUG_ON (tcp_socket_init (sb_new, sys_fd));

	/* BUG: if fault */
	return acceptq_add (sb, sb_new);
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
