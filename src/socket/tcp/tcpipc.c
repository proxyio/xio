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
#include "../sock.h"

static i64 ti_connector_read (struct io *ops, char *buff, i64 sz)
{
	struct tcpipc_sock *self = cont_of (ops, struct tcpipc_sock, ops);
	struct transport *vtp = self->vtp;

	BUG_ON (!vtp);
	int rc = vtp->recv (self->sys_fd, buff, sz);
	return rc;
}

static i64 ti_connector_write (struct io *ops, char *buff, i64 sz)
{
	struct tcpipc_sock *self = cont_of (ops, struct tcpipc_sock, ops);
	struct transport *vtp = self->vtp;

	BUG_ON (!vtp);
	int rc = vtp->send (self->sys_fd, buff, sz);
	return rc;
}

struct io default_xops = {
	.read = ti_connector_read,
	.write = ti_connector_write,
};



/******************************************************************************
 *  snd_head events trigger.
 ******************************************************************************/

static void snd_head_empty (struct sockbase *sb)
{
	struct tcpipc_sock *self = cont_of (sb, struct tcpipc_sock, base);
	struct task_runner *cpu = get_task_runner (sb->cpu_no);
	int64_t sndbuf = bio_size (&self->out);

	// Disable POLLOUT event when snd_head is empty
	BUG_ON (sndbuf < 0);
	if (bio_size (&self->out) == 0 && (self->et.events & EPOLLOUT) ) {
		DEBUG_OFF ("%d disable EPOLLOUT", sb->fd);
		self->et.events &= ~EPOLLOUT;
		BUG_ON (eloop_mod (&cpu->el, &self->et) != 0);
	}
}

static void snd_head_nonempty (struct sockbase *sb)
{
	struct tcpipc_sock *self = cont_of (sb, struct tcpipc_sock, base);
	struct task_runner *cpu = get_task_runner (sb->cpu_no);

	// Enable POLLOUT event when snd_head isn't empty
	if (! (self->et.events & EPOLLOUT) ) {
		DEBUG_OFF ("%d enable EPOLLOUT", sb->fd);
		self->et.events |= EPOLLOUT;
		BUG_ON (eloop_mod (&cpu->el, &self->et) != 0);
	}
}


/******************************************************************************
 *  rcv_head events trigger.
 ******************************************************************************/

static void rcv_head_pop (struct sockbase *sb)
{
	if (sb->snd.waiters)
		condition_signal (&sb->cond);
}

static void rcv_head_full (struct sockbase *sb)
{
	struct tcpipc_sock *self = cont_of (sb, struct tcpipc_sock, base);
	struct task_runner *cpu = get_task_runner (sb->cpu_no);

	// Enable POLLOUT event when snd_head isn't empty
	if ( (self->et.events & EPOLLIN) ) {
		DEBUG_OFF ("%d disable EPOLLIN", sb->fd);
		self->et.events &= ~EPOLLIN;
		BUG_ON (eloop_mod (&cpu->el, &self->et) != 0);
	}
}

static void rcv_head_nonfull (struct sockbase *sb)
{
	struct tcpipc_sock *self = cont_of (sb, struct tcpipc_sock, base);
	struct task_runner *cpu = get_task_runner (sb->cpu_no);

	// Enable POLLOUT event when snd_head isn't empty
	if (! (self->et.events & EPOLLIN) ) {
		DEBUG_OFF ("%d enable EPOLLIN", sb->fd);
		self->et.events |= EPOLLIN;
		BUG_ON (eloop_mod (&cpu->el, &self->et) != 0);
	}
}

int ti_connector_hndl (eloop_t *el, ev_t *et);

struct sockbase *ti_alloc() {
	struct tcpipc_sock *self = TNEW (struct tcpipc_sock);

	if (self) {
		sockbase_init (&self->base);
		bio_init (&self->in);
		bio_init (&self->out);
		INIT_LIST_HEAD (&self->sg_head);
		return &self->base;
	}
	return 0;
}

static int ti_connector_bind (struct sockbase *sb, const char *sock)
{
	struct tcpipc_sock *self = cont_of (sb, struct tcpipc_sock, base);
	struct task_runner *cpu = get_task_runner (sb->cpu_no);
	int sys_fd;
	int on = 1;
	int blen = max (default_sndbuf, default_rcvbuf);

	BUG_ON (!cpu);
	BUG_ON (! (self->vtp = transport_lookup (sb->vfptr->pf) ) );
	if ( (sys_fd = self->vtp->connect (sock) ) < 0)
		return -1;
	BUG_ON (self->vtp->setopt (sys_fd, TP_NOBLOCK, &on, sizeof (on) ) );
	self->vtp->setopt (sys_fd, TP_SNDBUF, &blen, sizeof (blen) );
	self->vtp->setopt (sys_fd, TP_RCVBUF, &blen, sizeof (blen) );

	strncpy (sb->peer, sock, TP_SOCKADDRLEN);
	self->sys_fd = sys_fd;
	self->ops = default_xops;
	self->et.events = EPOLLIN|EPOLLRDHUP|EPOLLERR|EPOLLHUP;
	self->et.fd = sys_fd;
	self->et.f = ti_connector_hndl;
	self->et.data = self;
	BUG_ON (eloop_add (&cpu->el, &self->et) != 0);
	return 0;
}

static int ti_connector_snd (struct sockbase *sb);

static void ti_connector_close (struct sockbase *sb)
{
	struct tcpipc_sock *self = cont_of (sb, struct tcpipc_sock, base);
	struct task_runner *cpu = get_task_runner (sb->cpu_no);

	BUG_ON (!self->vtp);

	/* Try flush buf massage into network before close */
	ti_connector_snd (sb);

	/* Detach sock low-level file descriptor from poller */
	BUG_ON (eloop_del (&cpu->el, &self->et) != 0);
	self->vtp->close (self->sys_fd);

	self->sys_fd = -1;
	self->et.events = -1;
	self->et.fd = -1;
	self->et.f = 0;
	self->et.data = 0;
	self->vtp = 0;

	/* Destroy the sock base and free sockid. */
	sockbase_exit (sb);
	mem_free (self, sizeof (*self) );
}


static void rcv_head_notify (struct sockbase *sb, u32 events)
{
	if (events & XMQ_POP)
		rcv_head_pop (sb);
	if (events & XMQ_FULL)
		rcv_head_full (sb);
	else if (events & XMQ_NONFULL)
		rcv_head_nonfull (sb);
}

static void snd_head_notify (struct sockbase *sb, u32 events)
{
	if (events & XMQ_EMPTY)
		snd_head_empty (sb);
	else if (events & XMQ_NONEMPTY)
		snd_head_nonempty (sb);
}

static void ti_connector_notify (struct sockbase *sb, int type, u32 events)
{
	switch (type) {
	case RECV_Q:
		rcv_head_notify (sb, events);
		break;
	case SEND_Q:
		snd_head_notify (sb, events);
		break;
	default:
		BUG_ON (1);
	}
}

static int bufio_check_msg (struct bio *b)
{
	struct skbuf aim = {};

	if (b->bsize < sizeof (aim.chunk) )
		return false;
	bio_copy (b, (char *) (&aim.chunk), sizeof (aim.chunk) );
	if (b->bsize < skbuf_len (&aim) + (u32) aim.chunk.cmsg_length)
		return false;
	return true;
}


static void bufio_rm (struct bio *b, struct skbuf **msg)
{
	struct skbuf one = {};

	bio_copy (b, (char *) (&one.chunk), sizeof (one.chunk) );
	*msg = xalloc_skbuf (one.chunk.iov_len);
	bio_read (b, skbuf_base (*msg), skbuf_len (*msg) );
}

static int ti_connector_rcv (struct sockbase *sb)
{
	struct tcpipc_sock *self = cont_of (sb, struct tcpipc_sock, base);
	int rc = 0;
	u16 cmsg_num;
	struct skbuf *aim = 0, *cmsg = 0;

	rc = bio_prefetch (&self->in, &self->ops);
	if (rc < 0 && errno != EAGAIN)
		return rc;
	while (bufio_check_msg (&self->in) ) {
		aim = 0;
		bufio_rm (&self->in, &aim);
		BUG_ON (!aim);
		cmsg_num = aim->chunk.cmsg_num;
		while (cmsg_num--) {
			cmsg = 0;
			bufio_rm (&self->in, &cmsg);
			BUG_ON (!cmsg);
			list_add_tail (&cmsg->item, &aim->cmsg_head);
		}
		recvq_add (sb, aim);
		DEBUG_OFF ("%d sock recv one message", sb->fd);
	}
	return rc;
}

static void bufio_add (struct bio *b, struct skbuf *msg)
{
	struct list_head head = {};
	struct skbuf *nmsg;

	INIT_LIST_HEAD (&head);
	skbuf_serialize (msg, &head);

	walk_msg_s (msg, nmsg, &head) {
		bio_write (b, skbuf_base (msg), skbuf_len (msg) );
		xfree_skbuf (msg);
	}
}

static int sg_send (struct sockbase *sb)
{
	struct tcpipc_sock *self = cont_of (sb, struct tcpipc_sock, base);
	int rc;
	struct iovec *iov;
	struct skbuf *msg;
	struct msghdr msghdr = {};

	/* First. sending the bufio caching */
	if (bio_size (&self->out) > 0) {
		if ( (rc = bio_flush (&self->out, &self->ops) ) < 0)
			return rc;
		if (bio_size (&self->out) > 0) {
			errno = EAGAIN;
			return -1;
		}
	}
	/* Second. sending the scatter-gather iovec */
	BUG_ON (bio_size (&self->out) );
	if (!self->iov_length)
		return 0;
	msghdr.msg_iov = self->biov + self->iov_start;
	msghdr.msg_iovlen = self->iov_end - self->iov_start;
	if ( (rc = self->vtp->sendmsg (self->sys_fd, &msghdr, 0) ) < 0)
		return rc;
	iov = &self->biov[self->iov_start];
	while (rc >= iov->iov_len && iov < &self->biov[self->iov_end]) {
		rc -= iov->iov_len;
		msg = cont_of (iov->iov_base, struct skbuf, chunk);
		list_del_init (&msg->item);
		xfree_skbuf (msg);
		iov++;
	}
	/* Cache the reset iovec into bufio  */
	if (rc > 0) {
		bio_write (&self->out, iov->iov_base + rc, iov->iov_len - rc);
		msg = cont_of (iov->iov_base, struct skbuf, chunk);
		list_del_init (&msg->item);
		xfree_skbuf (msg);
		rc = 0;
		iov++;
	}
	self->iov_start = iov - self->biov;
	BUG_ON (iov > &self->biov[self->iov_end]);
	if (bio_size (&self->out) || self->iov_start < self->iov_end) {
		errno = EAGAIN;
		return -1;
	}
	if (self->iov_length > NELEM (self->iov, struct iovec) )
		mem_free (self->biov, self->iov_length * sizeof (struct iovec) );
	self->biov = 0;
	self->iov_start = self->iov_end = self->iov_length = 0;
	return 0;
}

static int ti_connector_sg (struct sockbase *sb)
{
	struct tcpipc_sock *self = cont_of (sb, struct tcpipc_sock, base);
	int rc;
	struct skbuf *msg, *nmsg;
	struct iovec *iov;

	while ( (rc = sg_send (sb) ) == 0) {
		BUG_ON (!list_empty (&self->sg_head) );

		/* Third. serialize the queue message for send */
		while ( (msg = sendq_rm (sb) ) )
			self->iov_length += skbuf_serialize (msg, &self->sg_head);
		if (self->iov_length <= 0) {
			errno = EAGAIN;
			return -1;
		}
		self->iov_end = self->iov_length;
		if (self->iov_length <= NELEM (self->iov, struct iovec) ) {
			self->biov = &self->iov[0];
		} else {
			/* BUG here ? */
			self->biov = NTNEW (struct iovec, self->iov_length);
			BUG_ON (!self->biov);
		}
		iov = self->biov;
		walk_msg_s (msg, nmsg, &self->sg_head) {
			list_del_init (&msg->item);
			iov->iov_base = skbuf_base (msg);
			iov->iov_len = skbuf_len (msg);
			iov++;
		}
	}
	return rc;
}

static int ti_connector_snd (struct sockbase *sb)
{
	struct tcpipc_sock *self = cont_of (sb, struct tcpipc_sock, base);
	int rc;
	struct skbuf *msg;

	if (self->vtp->sendmsg)
		return ti_connector_sg (sb);
	while ( (msg = sendq_rm (sb) ) )
		bufio_add (&self->out, msg);
	rc = bio_flush (&self->out, &self->ops);
	return rc;
}

int ti_connector_hndl (eloop_t *el, ev_t *et)
{
	int rc = 0;
	struct tcpipc_sock *self = cont_of (et, struct tcpipc_sock, et);
	struct sockbase *sb = &self->base;

	if (et->happened & EPOLLIN) {
		DEBUG_OFF ("io sock %d EPOLLIN", sb->fd);
		rc = ti_connector_rcv (sb);
	}
	if (et->happened & EPOLLOUT) {
		DEBUG_OFF ("io sock %d EPOLLOUT", sb->fd);
		rc = ti_connector_snd (sb);
	}
	if ( (rc < 0 && errno != EAGAIN) ||
	     et->happened & (EPOLLERR|EPOLLRDHUP|EPOLLHUP) ) {
		DEBUG_OFF ("io sock %d EPIPE with events %d", sb->fd, et->happened);
		mutex_lock (&sb->lock);
		sb->fepipe = true;
		if (sb->rcv.waiters || sb->snd.waiters)
			condition_broadcast (&sb->cond);
		mutex_unlock (&sb->lock);
	}
	emit_pollevents (sb);
	return rc;
}



struct sockbase_vfptr tcp_connector_spec = {
	.type = XCONNECTOR,
	.pf = XPF_TCP,
	.alloc = ti_alloc,
	.bind = ti_connector_bind,
	.close = ti_connector_close,
	.notify = ti_connector_notify,
	.setopt = 0,
	.getopt = 0,
};

struct sockbase_vfptr ipc_connector_spec = {
	.type = XCONNECTOR,
	.pf = XPF_IPC,
	.alloc = ti_alloc,
	.bind = ti_connector_bind,
	.close = ti_connector_close,
	.notify = ti_connector_notify,
	.setopt = 0,
	.getopt = 0,
};
