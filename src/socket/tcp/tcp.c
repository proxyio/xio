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

static i64 tcp_connector_read (struct io *ops, char *buff, i64 sz)
{
	struct tcp_sock *tcps = cont_of (ops, struct tcp_sock, ops);
	int rc = rex_sock_recv (&tcps->s, buff, sz);
	return rc;
}

static i64 tcp_connector_write (struct io *ops, char *buff, i64 sz)
{
	struct tcp_sock *tcps = cont_of (ops, struct tcp_sock, ops);
	int rc = rex_sock_send (&tcps->s, buff, sz);
	return rc;
}

struct io stream_ops = {
	.read = tcp_connector_read,
	.write = tcp_connector_write,
};

static void snd_msgbuf_head_empty_ev_hndl (struct sockbase *sb)
{
	struct tcp_sock *tcps = cont_of (sb, struct tcp_sock, base);
	struct ev_loop *evl = sb->evl;

	mutex_lock (&sb->lock);
	/* Disable POLLOUT event when snd_head is empty */
	if (msgbuf_head_empty(&sb->snd) && bio_size (&tcps->out) == 0 &&
	    (tcps->et.events & EV_WRITE)) {
		SKLOG_DEBUG (sb, "%d disable EV_WRITE", sb->fd);
		tcps->et.events &= ~EV_WRITE;
		BUG_ON (__ev_fdset_ctl (&evl->fdset, EV_MOD, &tcps->et) != 0);
	}
	mutex_unlock (&sb->lock);
}

static void snd_msgbuf_head_nonempty_ev_hndl (struct sockbase *sb)
{
	struct tcp_sock *tcps = cont_of (sb, struct tcp_sock, base);
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
	struct tcp_sock *tcps = cont_of (sb, struct tcp_sock, base);
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
	struct tcp_sock *tcps = cont_of (sb, struct tcp_sock, base);
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

void tcp_connector_hndl (struct ev_fdset *evfds, struct ev_fd *evfd, int events);



static struct tcp_sock *__tcp_alloc ()
{
	struct tcp_sock *tcps = TNEW (struct tcp_sock);
	struct sockbase *sb = &tcps->base;

	sockbase_init (sb);
	bio_init (&tcps->in);
	bio_init (&tcps->out);
	INIT_LIST_HEAD (&tcps->sg_head);
	ev_fd_init (&tcps->et);
	return tcps;
}

struct sockbase *tcp_alloc ()
{
	struct tcp_sock *tcps = __tcp_alloc ();
	rex_sock_init (&tcps->s, REX_AF_TCP);
	return &tcps->base;
}

struct sockbase *ipc_alloc ()
{
	struct tcp_sock *tcps = __tcp_alloc ();
	rex_sock_init (&tcps->s, REX_AF_LOCAL);
	return &tcps->base;
}

static void tcp_signal (struct sockbase *sb, int signo)
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

static int tcp_send (struct sockbase *sb, char *ubuf)
{
	int rc;
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, chunk.ubuf_base);

	if ((rc = snd_msgbuf_head_add (sb, msg)) < 0)
		errno = sb->flagset.epipe ? EPIPE : EAGAIN;
	return rc;
}

void tcp_socket_init (struct tcp_sock *tcps)
{
	struct sockbase *sb = &tcps->base;
	struct ev_loop *evl = sb->evl;
	int on = 1;

	rex_sock_setopt (&tcps->s, REX_SO_NOBLOCK, &on, sizeof (on));
	tcps->ops = stream_ops;
	tcps->et.events = EV_READ;
	tcps->et.fd = tcps->s.ss_fd;
	tcps->et.hndl = tcp_connector_hndl;
}

static int tcp_connector_bind (struct sockbase *sb, const char *sock)
{
	struct tcp_sock *tcps = cont_of (sb, struct tcp_sock, base);
	int rc;

	if ((rc = rex_sock_connect (&tcps->s, sock)) < 0)
		return -1;
	strcpy (sb->peer, sock);
	tcp_socket_init (tcps);
	rc = ev_fdset_ctl (&sb->evl->fdset, EV_ADD, &tcps->et);
	return rc;
}

static int tcp_connector_snd (struct sockbase *sb);

static void tcp_connector_close (struct sockbase *sb)
{
	struct tcp_sock *tcps;
	struct ev_loop *evl;

	evl = sb->evl;
	tcps = cont_of (sb, struct tcp_sock, base);

	/* Detach sock low-level file descriptor from poller */
	if (tcps->s.ss_fd > 0)
		BUG_ON (ev_fdset_ctl (&evl->fdset, EV_DEL, &tcps->et) != 0);

	/* Try flush buf massage into network before close */
	tcp_connector_snd (sb);
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

static int tcp_connector_rcv (struct sockbase *sb)
{
	struct tcp_sock *tcps = cont_of (sb, struct tcp_sock, base);
	int rc = 0;
	u16 cmsg_num;
	struct msgbuf *aim = 0, *cmsg = 0;

	rc = bio_prefetch (&tcps->in, &tcps->ops);
	if (rc < 0 && errno != EAGAIN)
		return rc;
	while (bufio_check_msg (&tcps->in)) {
		aim = 0;
		bufio_rm (&tcps->in, &aim);
		BUG_ON (!aim);
		cmsg_num = aim->chunk.cmsg_num;
		while (cmsg_num--) {
			cmsg = 0;
			bufio_rm (&tcps->in, &cmsg);
			BUG_ON (!cmsg);
			list_add_tail (&cmsg->item, &aim->cmsg_head);
		}
		rcv_msgbuf_head_add (sb, aim);
		SKLOG_DEBUG (sb, "%d sock recv one message", sb->fd);
	}
	return rc;
}

static void bufio_add (struct bio *b, struct msgbuf *msg)
{
	struct list_head head = {};
	struct msgbuf *nmsg;

	INIT_LIST_HEAD (&head);
	msgbuf_serialize (msg, &head);

	walk_msg_s (msg, nmsg, &head) {
		bio_write (b, msgbuf_base (msg), msgbuf_len (msg));
		msgbuf_free (msg);
	}
}

static int sg_send (struct sockbase *sb)
{
	struct tcp_sock *tcps = cont_of (sb, struct tcp_sock, base);
	int rc;
	struct msgbuf *msg;
	struct rex_iov *iov;
	

	/* First. sending the bufio caching */
	if (bio_size (&tcps->out) > 0) {
		if ((rc = bio_flush (&tcps->out, &tcps->ops)) < 0)
			return rc;
		if (bio_size (&tcps->out) > 0) {
			errno = EAGAIN;
			return -1;
		}
	}
	/* Second. sending the scatter-gather iovec */
	BUG_ON (bio_size (&tcps->out));
	if (!tcps->iov_length)
		return 0;
	if ((rc = rex_sock_sendv (&tcps->s, tcps->biov + tcps->iov_start,
				  tcps->iov_end - tcps->iov_start)) < 0)
		return rc;
	iov = &tcps->biov[tcps->iov_start];
	while (rc >= iov->iov_len && iov < &tcps->biov[tcps->iov_end]) {
		rc -= iov->iov_len;
		msg = cont_of (iov->iov_base, struct msgbuf, chunk);
		list_del_init (&msg->item);
		msgbuf_free (msg);
		iov++;
	}
	/* Cache the reset iovec into bufio  */
	if (rc > 0) {
		bio_write (&tcps->out, iov->iov_base + rc, iov->iov_len - rc);
		msg = cont_of (iov->iov_base, struct msgbuf, chunk);
		list_del_init (&msg->item);
		msgbuf_free (msg);
		rc = 0;
		iov++;
	}
	tcps->iov_start = iov - tcps->biov;
	BUG_ON (iov > &tcps->biov[tcps->iov_end]);
	if (bio_size (&tcps->out) || tcps->iov_start < tcps->iov_end) {
		errno = EAGAIN;
		return -1;
	}
	if (tcps->iov_length > NELEM (tcps->iov, struct rex_iov))
		mem_free (tcps->biov, tcps->iov_length * sizeof (struct rex_iov));
	tcps->biov = 0;
	tcps->iov_start = tcps->iov_end = tcps->iov_length = 0;
	return 0;
}

static int tcp_connector_sg (struct sockbase *sb)
{
	struct tcp_sock *tcps = cont_of (sb, struct tcp_sock, base);
	int rc;
	struct msgbuf *msg, *nmsg;
	struct rex_iov *iov;

	while ((rc = sg_send (sb)) == 0) {
		BUG_ON (!list_empty (&tcps->sg_head));

		/* Third. serialize the queue message for send */
		while ((msg = snd_msgbuf_head_rm (sb)) )
			tcps->iov_length += msgbuf_serialize (msg, &tcps->sg_head);
		if (tcps->iov_length <= 0) {
			errno = EAGAIN;
			return -1;
		}
		tcps->iov_end = tcps->iov_length;
		if (tcps->iov_length <= NELEM (tcps->iov, struct rex_iov)) {
			tcps->biov = &tcps->iov[0];
		} else {
			/* BUG here ? */
			tcps->biov = NTNEW (struct rex_iov, tcps->iov_length);
			BUG_ON (!tcps->biov);
		}
		iov = tcps->biov;
		walk_msg_s (msg, nmsg, &tcps->sg_head) {
			list_del_init (&msg->item);
			iov->iov_base = msgbuf_base (msg);
			iov->iov_len = msgbuf_len (msg);
			iov++;
		}
	}
	return rc;
}

static int tcp_connector_snd (struct sockbase *sb)
{
	struct tcp_sock *tcps = cont_of (sb, struct tcp_sock, base);
	int rc;
	struct msgbuf *msg;

	tcp_connector_sg (sb);
	while ((msg = snd_msgbuf_head_rm (sb)) )
		bufio_add (&tcps->out, msg);
	rc = bio_flush (&tcps->out, &tcps->ops);
	return rc;
}

void tcp_connector_hndl (struct ev_fdset *evfds, struct ev_fd *evfd, int events)
{
	int rc = 0;
	struct sockbase *sb = &cont_of (evfd, struct tcp_sock, et)->base;

	if (events & EV_READ) {
		SKLOG_DEBUG (sb, "io sock %d EV_READ", sb->fd);
		rc = tcp_connector_rcv (sb);
	}
	if (events & EV_WRITE) {
		SKLOG_DEBUG (sb, "io sock %d EV_WRITE", sb->fd);
		rc = tcp_connector_snd (sb);
	}
	if (rc < 0 && errno != EAGAIN) {
		SKLOG_DEBUG (sb, "io sock %d EPIPE with events %d", sb->fd, events);
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
	.pf = TP_TCP,
	.alloc = tcp_alloc,
	.signal = tcp_signal,
	.send = tcp_send,
	.bind = tcp_connector_bind,
	.close = tcp_connector_close,
	.setopt = 0,
	.getopt = 0,
};

struct sockbase_vfptr ipc_connector_vfptr = {
	.type = XCONNECTOR,
	.pf = TP_IPC,
	.alloc = ipc_alloc,
	.signal = tcp_signal,
	.send = tcp_send,
	.bind = tcp_connector_bind,
	.close = tcp_connector_close,
	.setopt = 0,
	.getopt = 0,
};
