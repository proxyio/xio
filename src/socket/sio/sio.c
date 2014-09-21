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

static i64 socket_read (struct io *ops, char *buf, i64 size)
{
	struct sio *tcps = cont_of (ops, struct sio, ops);
	i64 rc = rex_sock_recv (&tcps->s, buf, size);
	SKLOG_NOTICE (&tcps->base, "%d sock recv %"PRId64" bytes from network", tcps->base.fd, rc);
	return rc;
}

struct io sio_ops = {
	.read = socket_read,
	.write = 0,
};

void sio_connector_hndl (struct ev_fdset *evfds, struct ev_fd *evfd, int events);

static struct sio *salloc ()
{
	struct sio *tcps = mem_zalloc (sizeof (struct sio));

	sockbase_init (&tcps->base);
	ev_sig_init (&tcps->sig, sio_usignal_hndl);
	tcps->el = ev_get_loop_lla ();
	msgbuf_head_ev_hndl (&tcps->base.rcv, &tcps_rcvhead_vfptr);
	msgbuf_head_ev_hndl (&tcps->base.snd, &tcps_sndhead_vfptr);
	
	ev_fd_init (&tcps->et);
	bio_init (&tcps->rbuf);
	return tcps;
}

struct sockbase *tcp_open ()
{
	struct sio *tcps = salloc ();
	rex_sock_init (&tcps->s, REX_AF_TCP);
	return &tcps->base;
}

struct sockbase *ipc_open ()
{
	struct sio *tcps = salloc ();
	rex_sock_init (&tcps->s, REX_AF_LOCAL);
	return &tcps->base;
}


static int sio_connector_send (struct sockbase *sb, char *ubuf)
{
	int rc;
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, chunk.ubuf_base);

	if ((rc = snd_msgbuf_head_add (sb, msg)) < 0)
		errno = sb->flagset.epipe ? EPIPE : EAGAIN;
	return rc;
}

void sio_init (struct sio *tcps)
{
	struct sockbase *sb = &tcps->base;
	struct ev_loop *el = tcps->el;
	int on = 1;

	rex_sock_setopt (&tcps->s, REX_SO_NOBLOCK, &on, sizeof (on));
	tcps->ops = sio_ops;
	tcps->et.events = EV_READ;
	tcps->et.fd = tcps->s.ss_fd;
	tcps->et.hndl = sio_connector_hndl;
	tcps->flagset.binded = 0;
}

static int sio_connector_bind (struct sockbase *sb, const char *sock)
{
	struct sio *tcps = cont_of (sb, struct sio, base);
	int rc = 0;

	if ((rc = rex_sock_connect (&tcps->s, sock)) < 0)
		return -1;
	strcpy (sb->peer, sock);
	sio_init (tcps);
	BUG_ON (ev_fdset_ctl (&tcps->el->fdset, EV_ADD, &tcps->et) != 0);
	BUG_ON (ev_fdset_sighndl (&tcps->el->fdset, &tcps->sig) != 0);
	tcps->flagset.binded = 1;
	return rc;
}

static int sio_connector_snd (struct sockbase *sb);

static void sio_connector_close (struct sockbase *sb)
{
	struct sio *tcps;
	struct msgbuf *msg, *tmp;

	tcps = cont_of (sb, struct sio, base);


	/* Detach sock low-level file descriptor from poller */
	if (tcps->flagset.binded) {
		BUG_ON (ev_fdset_unsighndl (&tcps->el->fdset, &tcps->sig) != 0);
		BUG_ON (ev_fdset_ctl (&tcps->el->fdset, EV_DEL, &tcps->et) != 0);

		/* Try flush buf massage into network before close */
		sio_connector_snd (sb);
		rex_sock_destroy (&tcps->s);
	}

	ev_sig_term (&tcps->sig);

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
	*msg = msgbuf_alloc (one.chunk.ulength);
	bio_read (b, msgbuf_base (*msg), msgbuf_len (*msg));
}

static int sio_connector_rcv (struct sockbase *sb)
{
	struct sio *tcps = cont_of (sb, struct sio, base);
	int nbytes;
	u16 cmsg_num;
	struct msgbuf *msg = 0, *cmsg = 0;

	if ((nbytes = bio_prefetch (&tcps->rbuf, &tcps->ops)) < 0 && errno != EAGAIN)
		return -1;
	while (bufio_check_msg (&tcps->rbuf)) {
		SKLOG_NOTICE (sb, "%d sock recv msg from network", sb->fd);
		msg = 0;
		bufio_rm (&tcps->rbuf, &msg);
		BUG_ON (!msg);
		cmsg_num = msg->chunk.cmsg_num;
		while (cmsg_num--) {
			cmsg = 0;
			bufio_rm (&tcps->rbuf, &cmsg);
			BUG_ON (!cmsg);
			list_add_tail (&cmsg->item, &msg->cmsg_head);
		}
		rcv_msgbuf_head_add (sb, msg);
	}
	return 0;
}


static int sndhead_preinstall_iovs (struct sockbase *sb, struct rex_iov *iovs, int n)
{
	int rc;
	mutex_lock (&sb->lock);
	rc = msgbuf_head_preinstall_iovs (&sb->snd, iovs, n);
	mutex_unlock (&sb->lock);
	return rc;
}

static void sndhead_install_iovs (struct sockbase *sb, struct rex_iov *iovs, i64 nbytes)
{
	int n;
	struct msgbuf *msg;

	mutex_lock (&sb->lock);
	n = msgbuf_head_install_iovs (&sb->snd, iovs, nbytes);
	while (n--) {
		BUG_ON (!(msg = __snd_msgbuf_head_rm (sb)));
		msgbuf_free (msg);
		SKLOG_NOTICE (sb, "%d sock send msg into network", sb->fd);
	}
	mutex_unlock (&sb->lock);
}
	
static int sio_connector_snd (struct sockbase *sb)
{
	struct sio *tcps = cont_of (sb, struct sio, base);
	i64 nbytes;
	int n;
	struct rex_iov iovs[100];

	if ((n = sndhead_preinstall_iovs (sb, iovs, NELEM (iovs, struct rex_iov))) == 0) {
		errno = EAGAIN;
		return -1;
	}
	if ((nbytes = rex_sock_sendv (&tcps->s, iovs, n)) < 0)
		return -1;
	SKLOG_NOTICE (sb, "%d sock send %"PRId64" bytes into network", sb->fd, nbytes);
	sndhead_install_iovs (sb, iovs, nbytes);
	return 0;
}

void sio_connector_hndl (struct ev_fdset *evfds, struct ev_fd *evfd, int events)
{
	int rc = 0;
	struct sockbase *sb = &cont_of (evfd, struct sio, et)->base;

	if (events & EV_READ) {
		SKLOG_DEBUG (sb, "%d sock EV_READ", sb->fd);
		rc = sio_connector_rcv (sb);
		socket_mstats_incr (&sb->stats, ST_EV_READ);
	}
	if (events & EV_WRITE) {
		SKLOG_DEBUG (sb, "%d sock EV_WRITE", sb->fd);
		rc = sio_connector_snd (sb);
		socket_mstats_incr (&sb->stats, ST_EV_WRITE);
	}
	if (rc < 0 && errno != EAGAIN) {
		SKLOG_DEBUG (sb, "%d sock EPIPE with events %d", sb->fd, events);
		mutex_lock (&sb->lock);
		sb->flagset.epipe = true;
		if (sb->rcv.waiters || sb->snd.waiters)
			condition_broadcast (&sb->cond);
		mutex_unlock (&sb->lock);
		socket_mstats_incr (&sb->stats, ST_EV_ERROR);
	}
	emit_pollevents (sb);
	mstats_base_emit (&sb->stats.base, gettimeofms ());
}


extern int xgeneric_recv (struct sockbase *sb, char **ubuf);

struct sockbase_vfptr tcp_connector_vfptr = {
	.type = XCONNECTOR,
	.pf = XAF_TCP,
	.open = tcp_open,
	.close = sio_connector_close,
	.bind = sio_connector_bind,
	.send = sio_connector_send,
	.recv = xgeneric_recv,
	.getopt = sio_getopt,
	.setopt = sio_setopt,
};

struct sockbase_vfptr ipc_connector_vfptr = {
	.type = XCONNECTOR,
	.pf = XAF_IPC,
	.open = ipc_open,
	.close = sio_connector_close,
	.bind = sio_connector_bind,
	.send = sio_connector_send,
	.recv = xgeneric_recv,
	.getopt = sio_getopt,
	.setopt = sio_setopt,
};
