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
#include "sio.h"

static i64 sio_connector_read (struct io *ops, char *buf, i64 size)
{
	struct sio_sock *tcps = cont_of (ops, struct sio_sock, ops);
	i64 rc = rex_sock_recv (&tcps->s, buf, size);
	SKLOG_NOTICE (&tcps->base, "%d sock recv %ld bytes from network", tcps->base.fd, rc);
	return rc;
}

struct io sio_ops = {
	.read = sio_connector_read,
	.write = 0,
};

static void snd_msgbuf_head_empty_ev_hndl (struct sockbase *sb)
{
	struct sio_sock *tcps = cont_of (sb, struct sio_sock, base);
	struct ev_loop *evl = sb->evl;

	mutex_lock (&sb->lock);
	/* Disable POLLOUT event when snd_head is empty */
	if (msgbuf_head_empty(&sb->snd) && list_empty (&tcps->un_head) &&
	    (tcps->et.events & EV_WRITE)) {
		SKLOG_DEBUG (sb, "%d disable EV_WRITE", sb->fd);
		tcps->et.events &= ~EV_WRITE;
		BUG_ON (__ev_fdset_ctl (&evl->fdset, EV_MOD, &tcps->et) != 0);
	}
	mutex_unlock (&sb->lock);
}

static void snd_msgbuf_head_nonempty_ev_hndl (struct sockbase *sb)
{
	struct sio_sock *tcps = cont_of (sb, struct sio_sock, base);
	struct ev_loop *evl = sb->evl;

	mutex_lock (&sb->lock);
	/* Enable POLLOUT event when snd_head isn't empty */
	if (!(tcps->et.events & EV_WRITE)) {
		SKLOG_DEBUG (sb, "%d enable EV_WRITE", sb->fd);
		tcps->et.events |= EV_WRITE;
		BUG_ON (__ev_fdset_ctl (&evl->fdset, EV_MOD, &tcps->et) != 0);
	}
	mutex_unlock (&sb->lock);
}

static void rcv_msgbuf_head_rm_ev_hndl (struct sockbase *sb)
{
	mutex_lock (&sb->lock);
	if (sb->snd.waiters)
		condition_signal (&sb->cond);
	mutex_unlock (&sb->lock);
}

static void rcv_msgbuf_head_full_ev_hndl (struct sockbase *sb)
{
	struct sio_sock *tcps = cont_of (sb, struct sio_sock, base);
	struct ev_loop *evl = sb->evl;

	mutex_lock (&sb->lock);
	/* Enable POLLOUT event when snd_head isn't empty */
	if ((tcps->et.events & EV_READ)) {
		SKLOG_DEBUG (sb, "%d disable EV_READ", sb->fd);
		tcps->et.events &= ~EV_READ;
		BUG_ON (__ev_fdset_ctl (&evl->fdset, EV_MOD, &tcps->et) != 0);
	}
	mutex_unlock (&sb->lock);
}


static void rcv_msgbuf_head_nonfull_ev_hndl (struct sockbase *sb)
{
	struct sio_sock *tcps = cont_of (sb, struct sio_sock, base);
	struct ev_loop *evl = sb->evl;

	mutex_lock (&sb->lock);
	/* Enable POLLOUT event when snd_head isn't empty */
	if (!(tcps->et.events & EV_READ)) {
		SKLOG_DEBUG (sb, "%d enable EV_READ", sb->fd);
		tcps->et.events |= EV_READ;
		BUG_ON (__ev_fdset_ctl (&evl->fdset, EV_MOD, &tcps->et) != 0);
	}
	mutex_unlock (&sb->lock);
}

void sio_connector_hndl (struct ev_fdset *evfds, struct ev_fd *evfd, int events);

static struct sio_sock *__sio_alloc ()
{
	struct sio_sock *tcps = mem_zalloc (sizeof (struct sio_sock));

	sockbase_init (&tcps->base);
	ev_fd_init (&tcps->et);
	bio_init (&tcps->in);
	INIT_LIST_HEAD (&tcps->un_head);
	tcps->un_snd = NELEM (tcps->un_iov, struct rex_iov);
	return tcps;
}

struct sockbase *tcp_alloc ()
{
	struct sio_sock *tcps = __sio_alloc ();
	rex_sock_init (&tcps->s, REX_AF_TCP);
	return &tcps->base;
}

struct sockbase *ipc_alloc ()
{
	struct sio_sock *tcps = __sio_alloc ();
	rex_sock_init (&tcps->s, REX_AF_LOCAL);
	return &tcps->base;
}

static void sio_connector_signal (struct sockbase *sb, int signo)
{
	switch (signo) {
	case EV_SNDBUF_EMPTY:
		snd_msgbuf_head_empty_ev_hndl (sb);
		break;
	case EV_SNDBUF_NONEMPTY:
		snd_msgbuf_head_nonempty_ev_hndl (sb);
		break;
	case EV_RCVBUF_RM:
		rcv_msgbuf_head_rm_ev_hndl (sb);
		break;
	case EV_RCVBUF_FULL:
		rcv_msgbuf_head_full_ev_hndl (sb);
		break;
	case EV_RCVBUF_NONFULL:
		rcv_msgbuf_head_nonfull_ev_hndl (sb);
		break;
	}
}

static int sio_connector_send (struct sockbase *sb, char *ubuf)
{
	int rc;
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, chunk.ubuf_base);

	if ((rc = snd_msgbuf_head_add (sb, msg)) < 0)
		errno = sb->flagset.epipe ? EPIPE : EAGAIN;
	return rc;
}

void sio_socket_init (struct sio_sock *tcps)
{
	struct sockbase *sb = &tcps->base;
	struct ev_loop *evl = sb->evl;
	int on = 1;

	rex_sock_setopt (&tcps->s, REX_SO_NOBLOCK, &on, sizeof (on));
	tcps->ops = sio_ops;
	tcps->et.events = EV_READ;
	tcps->et.fd = tcps->s.ss_fd;
	tcps->et.hndl = sio_connector_hndl;
}

static int sio_connector_bind (struct sockbase *sb, const char *sock)
{
	struct sio_sock *tcps = cont_of (sb, struct sio_sock, base);
	int rc;

	if ((rc = rex_sock_connect (&tcps->s, sock)) < 0)
		return -1;
	strcpy (sb->peer, sock);
	sio_socket_init (tcps);
	rc = ev_fdset_ctl (&sb->evl->fdset, EV_ADD, &tcps->et);
	return rc;
}

static int sio_connector_snd (struct sockbase *sb);

static void sio_connector_close (struct sockbase *sb)
{
	struct sio_sock *tcps;
	struct ev_loop *evl;

	evl = sb->evl;
	tcps = cont_of (sb, struct sio_sock, base);

	/* Detach sock low-level file descriptor from poller */
	if (tcps->s.ss_fd > 0)
		BUG_ON (ev_fdset_ctl (&evl->fdset, EV_DEL, &tcps->et) != 0);

	/* Try flush buf massage into network before close */
	sio_connector_snd (sb);
	rex_sock_destroy (&tcps->s);

	/* Destroy the sock base and free sockid. */
	sockbase_exit (sb);
	mem_free (tcps, sizeof (*tcps));
}


static int bufio_check_msg (struct bio *b)
{
	struct msgbuf aim = {};

	if (b->bsize < sizeof (aim.chunk))
		return false;
	bio_copy (b, (char *) (&aim.chunk), sizeof (aim.chunk));
	if (b->bsize < msgbuf_len (&aim) + (u32) aim.chunk.cmsg_length)
		return false;
	return true;
}


static void bufio_rm (struct bio *b, struct msgbuf **msg)
{
	struct msgbuf one = {};

	bio_copy (b, (char *) (&one.chunk), sizeof (one.chunk));
	*msg = msgbuf_alloc (one.chunk.ubuf_len);
	bio_read (b, msgbuf_base (*msg), msgbuf_len (*msg));
}

static int sio_connector_rcv (struct sockbase *sb)
{
	struct sio_sock *tcps = cont_of (sb, struct sio_sock, base);
	int nbytes;
	u16 cmsg_num;
	struct msgbuf *msg = 0, *cmsg = 0;

	if ((nbytes = bio_prefetch (&tcps->in, &tcps->ops)) < 0 && errno != EAGAIN)
		return -1;
	while (bufio_check_msg (&tcps->in)) {
		SKLOG_NOTICE (sb, "%d sock recv msg from network", sb->fd);
		msg = 0;
		bufio_rm (&tcps->in, &msg);
		BUG_ON (!msg);
		cmsg_num = msg->chunk.cmsg_num;
		while (cmsg_num--) {
			cmsg = 0;
			bufio_rm (&tcps->in, &cmsg);
			BUG_ON (!cmsg);
			list_add_tail (&cmsg->item, &msg->cmsg_head);
		}
		rcv_msgbuf_head_add (sb, msg);
	}
	return 0;
}

static void fill_msgbuf_iovs (struct sio_sock *tcps)
{
	int n = NELEM (tcps->un_iov, struct rex_iov);
	int i = 0;
	struct msgbuf *msg, *tmp;

	walk_each_entry_s (msg, tmp, &tcps->un_head, struct msgbuf, item) {
		if (i < n - tcps->un_snd)
			continue;
		tcps->un_snd--;
		tcps->un_iov[i].iov_len = msgbuf_len (msg);
		tcps->un_iov[i].iov_base = msgbuf_base (msg);
		i++;
	}
}

static int sio_connector_snd (struct sockbase *sb)
{
	struct sio_sock *tcps = cont_of (sb, struct sio_sock, base);
	i64 nbytes;
	int i;
	int n;
	struct msgbuf *msg;

	n = NELEM (tcps->un_iov, struct rex_iov);
	if (tcps->un_snd > 0 && (msg = snd_msgbuf_head_rm (sb)))
		msgbuf_serialize (msg, &tcps->un_head);
	fill_msgbuf_iovs (tcps);
	if (tcps->un_snd == n) {
		errno = EAGAIN;
		return -1;   /* no available msgbuf needed send */
	}
	if ((nbytes = rex_sock_sendv (&tcps->s, tcps->un_iov, n - tcps->un_snd)) < 0)
		return -1;
	SKLOG_NOTICE (sb, "%d sock send %ld bytes into network", sb->fd, nbytes);
	for (i = 0; i < n - tcps->un_snd; i++) {
		if (nbytes < tcps->un_iov[i].iov_len) {
			tcps->un_iov[i].iov_base += nbytes;
			tcps->un_iov[i].iov_len -= nbytes;
			break;
		}
		msg = list_first (&tcps->un_head, struct msgbuf, item);
		list_del_init (&msg->item);
		msgbuf_free (msg);
		SKLOG_NOTICE (sb, "%d sock send msg into network", sb->fd);
	}
	tcps->un_snd += i;
	memmove (tcps->un_iov, tcps->un_iov + i, n - tcps->un_snd);
	return 0;
}

void sio_connector_hndl (struct ev_fdset *evfds, struct ev_fd *evfd, int events)
{
	int rc = 0;
	struct sockbase *sb = &cont_of (evfd, struct sio_sock, et)->base;

	if (events & EV_READ) {
		SKLOG_DEBUG (sb, "%d sock EV_READ", sb->fd);
		rc = sio_connector_rcv (sb);
	}
	if (events & EV_WRITE) {
		SKLOG_DEBUG (sb, "%d sock EV_WRITE", sb->fd);
		rc = sio_connector_snd (sb);
	}
	if (rc < 0 && errno != EAGAIN) {
		SKLOG_DEBUG (sb, "%d sock EPIPE with events %d", sb->fd, events);
		mutex_lock (&sb->lock);
		sb->flagset.epipe = true;
		if (sb->rcv.waiters || sb->snd.waiters)
			condition_broadcast (&sb->cond);
		mutex_unlock (&sb->lock);
	}
	emit_pollevents (sb);
}



struct sockbase_vfptr tcp_connector_vfptr = {
	.type = XCONNECTOR,
	.pf = XAF_TCP,
	.alloc = tcp_alloc,
	.signal = sio_connector_signal,
	.send = sio_connector_send,
	.bind = sio_connector_bind,
	.close = sio_connector_close,
	.setopt = 0,
	.getopt = 0,
};

struct sockbase_vfptr ipc_connector_vfptr = {
	.type = XCONNECTOR,
	.pf = XAF_IPC,
	.alloc = ipc_alloc,
	.signal = sio_connector_signal,
	.send = sio_connector_send,
	.bind = sio_connector_bind,
	.close = sio_connector_close,
	.setopt = 0,
	.getopt = 0,
};
