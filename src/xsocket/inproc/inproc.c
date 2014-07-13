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
#include <xsocket/xg.h>

extern struct sockbase *getlistener (const char *addr);

/* snd_head events trigger. */

static void snd_msgbuf_head_add_ev_hndl (struct msgbuf_head *bh)
{
	struct msgbuf *msg;
	struct sockbase *sb = cont_of (bh, struct sockbase, snd);
	struct sockbase *peer = (cont_of (sb, struct inproc_sock, base))->peer;

	/* TODO: maybe the peer sock can't recv anymore after the check. */
	mutex_unlock (&sb->lock);
	if ((msg = snd_msgbuf_head_rm (sb)))
		rcv_msgbuf_head_add (peer, msg);
	mutex_lock (&sb->lock);
}

/* rcv_head events trigger.
 */

static void rcv_msgbuf_head_rm_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, rcv);
	
	if (sb->snd.waiters)
		condition_signal (&sb->cond);
}


static struct sockbase *inproc_alloc ()
{
	struct sockbase *sb;
	struct inproc_sock *self = TNEW (struct inproc_sock);

	if (!self)
		return 0;
	sockbase_init (&self->base);
	atomic_init (&self->ref);
	atomic_incr (&self->ref);
	sb = &self->base;
	return sb;
}

static int inproc_send (struct sockbase *sb, char *ubuf)
{
	int rc;
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, chunk.ubuf_base);

	if ((rc = snd_msgbuf_head_add (sb, msg)) < 0)
		errno = sb->fepipe ? EPIPE : EAGAIN;
	return rc;
}


/* sock_inproc_vfptr
 */

static int inproc_connector_bind (struct sockbase *sb, const char *sock)
{
	int new_fd = 0;
	struct sockbase *new_sb = 0;
	struct sockbase *listener = getlistener (sock);
	struct inproc_sock *self = 0;
	struct inproc_sock *peer = 0;

	if (!listener) {
		errno = ECONNREFUSED;
		return -1;
	}
	if ((new_fd = xalloc (sb->vfptr->pf, sb->vfptr->type)) < 0) {
		errno = EMFILE;
		xput (listener->fd);
		return -1;
	}
	new_sb = xgb.sockbases[new_fd];
	self = cont_of (sb, struct inproc_sock, base);
	peer = cont_of (new_sb, struct inproc_sock, base);
	new_sb->vfptr = sb->vfptr;
	strcpy (sb->peer, sock);
	strcpy (new_sb->addr, sock);

	atomic_incr (&self->ref);
	atomic_incr (&peer->ref);
	self->peer = &peer->base;
	peer->peer = &self->base;

	msgbuf_head_handle_add (&sb->snd, snd_msgbuf_head_add_ev_hndl);
	msgbuf_head_handle_rm (&sb->rcv, rcv_msgbuf_head_rm_ev_hndl);
	msgbuf_head_handle_add (&new_sb->snd, snd_msgbuf_head_add_ev_hndl);
	msgbuf_head_handle_rm (&new_sb->rcv, rcv_msgbuf_head_rm_ev_hndl);
	
	if (acceptq_add (listener, new_sb) < 0) {
		atomic_decr (&peer->ref);
		sb->vfptr->close (new_sb);
		xput (listener->fd);
		errno = ECONNREFUSED;
		return -1;
	}
	xput (listener->fd);
	DEBUG_OFF ("%d accept new connection %d", sb->fd, new_fd);
	return 0;
}

static void inproc_peer_close (struct inproc_sock *peer)
{
	/* Destroy the sock and free sock id if i hold the last ref. */
	mutex_lock (&peer->base.lock);
	if (peer->base.rcv.waiters || peer->base.snd.waiters)
		condition_broadcast (&peer->base.cond);
	mutex_unlock (&peer->base.lock);

	if (atomic_decr (&peer->ref) == 1) {
		sockbase_exit (&peer->base);
		atomic_destroy (&peer->ref);
		mem_free (peer, sizeof (*peer));
	}
}

static void inproc_connector_close (struct sockbase *sb)
{
	struct inproc_sock *self = cont_of (sb, struct inproc_sock, base);
	struct inproc_sock *peer = 0;

	/* TODO: bug on here */
	if (self->peer) {
		peer = cont_of (self->peer, struct inproc_sock, base);
		inproc_peer_close (peer);
	}
	if (atomic_decr (&self->ref) == 1) {
		sockbase_exit (&self->base);
		atomic_destroy (&self->ref);
		mem_free (self, sizeof (*self));
	}
}

struct sockbase_vfptr inproc_connector_vfptr = {
	.type = XCONNECTOR,
	.pf = TP_INPROC,
	.alloc = inproc_alloc,
	.send = inproc_send,
	.bind = inproc_connector_bind,
	.close = inproc_connector_close,
	.getopt = 0,
	.setopt = 0,
};
