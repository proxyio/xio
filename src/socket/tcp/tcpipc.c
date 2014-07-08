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

static i64 tcp_connector_read (struct io *ops, char *buff, i64 sz)
{
	struct tcpipc_sock *tcpsk = cont_of (ops, struct tcpipc_sock, ops);
	struct transport *vtp = tcpsk->vtp;

	BUG_ON (!vtp);
	int rc = vtp->recv (tcpsk->sys_fd, buff, sz);
	return rc;
}

static i64 tcp_connector_write (struct io *ops, char *buff, i64 sz)
{
	struct tcpipc_sock *tcpsk = cont_of (ops, struct tcpipc_sock, ops);
	struct transport *vtp = tcpsk->vtp;

	BUG_ON (!vtp);
	int rc = vtp->send (tcpsk->sys_fd, buff, sz);
	return rc;
}

struct io default_xops = {
	.read = tcp_connector_read,
	.write = tcp_connector_write,
};

static void snd_msgbuf_head_empty_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, snd);
	struct tcpipc_sock *tcpsk = cont_of (sb, struct tcpipc_sock, base);
	struct worker *cpu = get_worker (sb->cpu_no);
	int64_t sndbuf = bio_size (&tcpsk->out);

	/* Disable POLLOUT event when snd_head is empty */
	BUG_ON (sndbuf < 0);
	if (bio_size (&tcpsk->out) == 0 && (tcpsk->et.events & EPOLLOUT) ) {
		DEBUG_OFF ("%d disable EPOLLOUT", sb->fd);
		tcpsk->et.events &= ~EPOLLOUT;
		BUG_ON (eloop_mod (&cpu->el, &tcpsk->et) != 0);
	}
}

static void snd_msgbuf_head_nonempty_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, snd);
	struct tcpipc_sock *tcpsk = cont_of (sb, struct tcpipc_sock, base);
	struct worker *cpu = get_worker (sb->cpu_no);

	/* Enable POLLOUT event when snd_head isn't empty */
	if (! (tcpsk->et.events & EPOLLOUT) ) {
		DEBUG_OFF ("%d enable EPOLLOUT", sb->fd);
		tcpsk->et.events |= EPOLLOUT;
		BUG_ON (eloop_mod (&cpu->el, &tcpsk->et) != 0);
	}
}

static void rcv_msgbuf_head_rm_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, rcv);

	if (sb->snd.waiters)
		condition_signal (&sb->cond);
}

static void rcv_msgbuf_head_full_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, rcv);
	struct tcpipc_sock *tcpsk = cont_of (sb, struct tcpipc_sock, base);
	struct worker *cpu = get_worker (sb->cpu_no);

	/* Enable POLLOUT event when snd_head isn't empty */
	if ( (tcpsk->et.events & EPOLLIN) ) {
		DEBUG_OFF ("%d disable EPOLLIN", sb->fd);
		tcpsk->et.events &= ~EPOLLIN;
		BUG_ON (eloop_mod (&cpu->el, &tcpsk->et) != 0);
	}
}

static void rcv_msgbuf_head_nonfull_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, rcv);
	struct tcpipc_sock *tcpsk = cont_of (sb, struct tcpipc_sock, base);
	struct worker *cpu = get_worker (sb->cpu_no);

	/* Enable POLLOUT event when snd_head isn't empty */
	if (! (tcpsk->et.events & EPOLLIN) ) {
		DEBUG_OFF ("%d enable EPOLLIN", sb->fd);
		tcpsk->et.events |= EPOLLIN;
		BUG_ON (eloop_mod (&cpu->el, &tcpsk->et) != 0);
	}
}

int tcp_connector_hndl (eloop_t *el, ev_t *et);

struct sockbase *tcp_alloc()
{
	struct tcpipc_sock *tcpsk = TNEW (struct tcpipc_sock);

	if (!tcpsk)
		return 0;
	sockbase_init (&tcpsk->base);
	bio_init (&tcpsk->in);
	bio_init (&tcpsk->out);
	INIT_LIST_HEAD (&tcpsk->sg_head);
	return &tcpsk->base;
}

static int tcp_send (struct sockbase *sb, char *ubuf)
{
	int rc;
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, chunk.ubuf_base);

	if ((rc = snd_msgbuf_head_add (sb, msg)) < 0)
		errno = sb->fepipe ? EPIPE : EAGAIN;
	return rc;
}

static int tcp_connector_bind (struct sockbase *sb, const char *sock)
{
	struct tcpipc_sock *tcpsk = cont_of (sb, struct tcpipc_sock, base);
	struct worker *cpu = get_worker (sb->cpu_no);
	int on = 1;
	int blen = max (default_sndbuf, default_rcvbuf);
	
	BUG_ON (!cpu);
	tcpsk->vtp = tp_get (sb->vfptr->pf);
	if ((tcpsk->sys_fd = tcpsk->vtp->connect (sock)) < 0)
		return -1;
	tcpsk->vtp->setopt (tcpsk->sys_fd, TP_NOBLOCK, &on, sizeof (on));
	tcpsk->vtp->setopt (tcpsk->sys_fd, TP_SNDBUF, &blen, sizeof (blen) );
	tcpsk->vtp->setopt (tcpsk->sys_fd, TP_RCVBUF, &blen, sizeof (blen) );

	strcpy (sb->peer, sock);
	tcpsk->ops = default_xops;
	tcpsk->et.events = EPOLLIN|EPOLLRDHUP|EPOLLERR|EPOLLHUP;
	tcpsk->et.fd = tcpsk->sys_fd;
	tcpsk->et.f = tcp_connector_hndl;
	tcpsk->et.data = tcpsk;
	BUG_ON (eloop_add (&cpu->el, &tcpsk->et) != 0);

	msgbuf_head_handle_rm (&sb->rcv, rcv_msgbuf_head_rm_ev_hndl);
	msgbuf_head_handle_full (&sb->rcv, rcv_msgbuf_head_full_ev_hndl);
	msgbuf_head_handle_nonfull (&sb->rcv, rcv_msgbuf_head_nonfull_ev_hndl);
	msgbuf_head_handle_empty (&sb->snd, snd_msgbuf_head_empty_ev_hndl);
	msgbuf_head_handle_nonempty (&sb->snd, snd_msgbuf_head_nonempty_ev_hndl);
	return 0;
}

static int tcp_connector_snd (struct sockbase *sb);

static void tcp_connector_close (struct sockbase *sb)
{
	struct tcpipc_sock *tcpsk = cont_of (sb, struct tcpipc_sock, base);
	struct worker *cpu = get_worker (sb->cpu_no);
	
	BUG_ON (!tcpsk->vtp);

	/* Try flush buf massage into network before close */
	tcp_connector_snd (sb);

	/* Detach sock low-level file descriptor from poller */
	if (tcpsk->sys_fd > 0) {
		BUG_ON (eloop_del (&cpu->el, &tcpsk->et) != 0);
		tcpsk->vtp->close (tcpsk->sys_fd);
	}

	tcpsk->sys_fd = -1;
	tcpsk->et.events = -1;
	tcpsk->et.fd = -1;
	tcpsk->et.f = 0;
	tcpsk->et.data = 0;
	tcpsk->vtp = 0;

	/* Destroy the sock base and free sockid. */
	sockbase_exit (sb);
	mem_free (tcpsk, sizeof (*tcpsk) );
}


static int bufio_check_msg (struct bio *b)
{
	struct msgbuf aim = {};

	if (b->bsize < sizeof (aim.chunk) )
		return false;
	bio_copy (b, (char *) (&aim.chunk), sizeof (aim.chunk) );
	if (b->bsize < msgbuf_len (&aim) + (u32) aim.chunk.cmsg_length)
		return false;
	return true;
}


static void bufio_rm (struct bio *b, struct msgbuf **msg)
{
	struct msgbuf one = {};

	bio_copy (b, (char *) (&one.chunk), sizeof (one.chunk) );
	*msg = msgbuf_alloc (one.chunk.ubuf_len);
	bio_read (b, msgbuf_base (*msg), msgbuf_len (*msg) );
}

static int tcp_connector_rcv (struct sockbase *sb)
{
	struct tcpipc_sock *tcpsk = cont_of (sb, struct tcpipc_sock, base);
	int rc = 0;
	u16 cmsg_num;
	struct msgbuf *aim = 0, *cmsg = 0;

	rc = bio_prefetch (&tcpsk->in, &tcpsk->ops);
	if (rc < 0 && errno != EAGAIN)
		return rc;
	while (bufio_check_msg (&tcpsk->in) ) {
		aim = 0;
		bufio_rm (&tcpsk->in, &aim);
		BUG_ON (!aim);
		cmsg_num = aim->chunk.cmsg_num;
		while (cmsg_num--) {
			cmsg = 0;
			bufio_rm (&tcpsk->in, &cmsg);
			BUG_ON (!cmsg);
			list_add_tail (&cmsg->item, &aim->cmsg_head);
		}
		rcv_msgbuf_head_add (sb, aim);
		DEBUG_OFF ("%d sock recv one message", sb->fd);
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
		bio_write (b, msgbuf_base (msg), msgbuf_len (msg) );
		msgbuf_free (msg);
	}
}

static int sg_send (struct sockbase *sb)
{
	struct tcpipc_sock *tcpsk = cont_of (sb, struct tcpipc_sock, base);
	int rc;
	struct iovec *iov;
	struct msgbuf *msg;
	struct msghdr msghdr = {};

	/* First. sending the bufio caching */
	if (bio_size (&tcpsk->out) > 0) {
		if ( (rc = bio_flush (&tcpsk->out, &tcpsk->ops) ) < 0)
			return rc;
		if (bio_size (&tcpsk->out) > 0) {
			errno = EAGAIN;
			return -1;
		}
	}
	/* Second. sending the scatter-gather iovec */
	BUG_ON (bio_size (&tcpsk->out) );
	if (!tcpsk->iov_length)
		return 0;
	msghdr.msg_iov = tcpsk->biov + tcpsk->iov_start;
	msghdr.msg_iovlen = tcpsk->iov_end - tcpsk->iov_start;
	if ( (rc = tcpsk->vtp->sendmsg (tcpsk->sys_fd, &msghdr, 0) ) < 0)
		return rc;
	iov = &tcpsk->biov[tcpsk->iov_start];
	while (rc >= iov->iov_len && iov < &tcpsk->biov[tcpsk->iov_end]) {
		rc -= iov->iov_len;
		msg = cont_of (iov->iov_base, struct msgbuf, chunk);
		list_del_init (&msg->item);
		msgbuf_free (msg);
		iov++;
	}
	/* Cache the reset iovec into bufio  */
	if (rc > 0) {
		bio_write (&tcpsk->out, iov->iov_base + rc, iov->iov_len - rc);
		msg = cont_of (iov->iov_base, struct msgbuf, chunk);
		list_del_init (&msg->item);
		msgbuf_free (msg);
		rc = 0;
		iov++;
	}
	tcpsk->iov_start = iov - tcpsk->biov;
	BUG_ON (iov > &tcpsk->biov[tcpsk->iov_end]);
	if (bio_size (&tcpsk->out) || tcpsk->iov_start < tcpsk->iov_end) {
		errno = EAGAIN;
		return -1;
	}
	if (tcpsk->iov_length > NELEM (tcpsk->iov, struct iovec) )
		mem_free (tcpsk->biov, tcpsk->iov_length * sizeof (struct iovec) );
	tcpsk->biov = 0;
	tcpsk->iov_start = tcpsk->iov_end = tcpsk->iov_length = 0;
	return 0;
}

static int tcp_connector_sg (struct sockbase *sb)
{
	struct tcpipc_sock *tcpsk = cont_of (sb, struct tcpipc_sock, base);
	int rc;
	struct msgbuf *msg, *nmsg;
	struct iovec *iov;

	while ( (rc = sg_send (sb) ) == 0) {
		BUG_ON (!list_empty (&tcpsk->sg_head) );

		/* Third. serialize the queue message for send */
		while ( (msg = snd_msgbuf_head_rm (sb) ) )
			tcpsk->iov_length += msgbuf_serialize (msg, &tcpsk->sg_head);
		if (tcpsk->iov_length <= 0) {
			errno = EAGAIN;
			return -1;
		}
		tcpsk->iov_end = tcpsk->iov_length;
		if (tcpsk->iov_length <= NELEM (tcpsk->iov, struct iovec) ) {
			tcpsk->biov = &tcpsk->iov[0];
		} else {
			/* BUG here ? */
			tcpsk->biov = NTNEW (struct iovec, tcpsk->iov_length);
			BUG_ON (!tcpsk->biov);
		}
		iov = tcpsk->biov;
		walk_msg_s (msg, nmsg, &tcpsk->sg_head) {
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
	struct tcpipc_sock *tcpsk = cont_of (sb, struct tcpipc_sock, base);
	int rc;
	struct msgbuf *msg;

	if (tcpsk->vtp->sendmsg)
		return tcp_connector_sg (sb);
	while ( (msg = snd_msgbuf_head_rm (sb) ) )
		bufio_add (&tcpsk->out, msg);
	rc = bio_flush (&tcpsk->out, &tcpsk->ops);
	return rc;
}

int tcp_connector_hndl (eloop_t *el, ev_t *et)
{
	int rc = 0;
	struct tcpipc_sock *tcpsk = cont_of (et, struct tcpipc_sock, et);
	struct sockbase *sb = &tcpsk->base;

	if (et->happened & EPOLLIN) {
		DEBUG_OFF ("io sock %d EPOLLIN", sb->fd);
		rc = tcp_connector_rcv (sb);
	}
	if (et->happened & EPOLLOUT) {
		DEBUG_ON ("io sock %d EPOLLOUT", sb->fd);
		rc = tcp_connector_snd (sb);
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
	.pf = TP_TCP,
	.alloc = tcp_alloc,
	.send = tcp_send,
	.bind = tcp_connector_bind,
	.close = tcp_connector_close,
	.setopt = 0,
	.getopt = 0,
};

struct sockbase_vfptr ipc_connector_spec = {
	.type = XCONNECTOR,
	.pf = TP_IPC,
	.alloc = tcp_alloc,
	.send = tcp_send,
	.bind = tcp_connector_bind,
	.close = tcp_connector_close,
	.setopt = 0,
	.getopt = 0,
};
