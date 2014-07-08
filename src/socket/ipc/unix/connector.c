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
#include <socket/global.h>

static i64 ipc_connector_read (struct io *ops, char *buff, i64 sz)
{
	struct ipc_sock *ipcsk = cont_of (ops, struct ipc_sock, ops);
	struct transport *vtp = ipcsk->vtp;

	BUG_ON (!vtp);
	int rc = vtp->recv (ipcsk->sys_fd, buff, sz);
	return rc;
}

static i64 ipc_connector_write (struct io *ops, char *buff, i64 sz)
{
	struct ipc_sock *ipcsk = cont_of (ops, struct ipc_sock, ops);
	struct transport *vtp = ipcsk->vtp;

	BUG_ON (!vtp);
	int rc = vtp->send (ipcsk->sys_fd, buff, sz);
	return rc;
}

struct io ipc_io_ops = {
	.read = ipc_connector_read,
	.write = ipc_connector_write,
};

static void snd_msgbuf_head_empty_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, snd);
	struct ipc_sock *ipcsk = cont_of (sb, struct ipc_sock, base);
	struct worker *cpu = get_worker (sb->cpu_no);
	int64_t sndbuf = bio_size (&ipcsk->out);

	/* Disable POLLOUT event when snd_head is empty */
	BUG_ON (sndbuf < 0);
	if (bio_size (&ipcsk->out) == 0 && (ipcsk->et.events & EPOLLOUT) ) {
		DEBUG_OFF ("%d disable EPOLLOUT", sb->fd);
		ipcsk->et.events &= ~EPOLLOUT;
		BUG_ON (eloop_mod (&cpu->el, &ipcsk->et) != 0);
	}
}

static void snd_msgbuf_head_nonempty_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, snd);
	struct ipc_sock *ipcsk = cont_of (sb, struct ipc_sock, base);
	struct worker *cpu = get_worker (sb->cpu_no);

	/* Enable POLLOUT event when snd_head isn't empty */
	if (! (ipcsk->et.events & EPOLLOUT) ) {
		DEBUG_OFF ("%d enable EPOLLOUT", sb->fd);
		ipcsk->et.events |= EPOLLOUT;
		BUG_ON (eloop_mod (&cpu->el, &ipcsk->et) != 0);
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
	struct ipc_sock *ipcsk = cont_of (sb, struct ipc_sock, base);
	struct worker *cpu = get_worker (sb->cpu_no);

	/* Enable POLLOUT event when snd_head isn't empty */
	if ( (ipcsk->et.events & EPOLLIN) ) {
		DEBUG_OFF ("%d disable EPOLLIN", sb->fd);
		ipcsk->et.events &= ~EPOLLIN;
		BUG_ON (eloop_mod (&cpu->el, &ipcsk->et) != 0);
	}
}

static void rcv_msgbuf_head_nonfull_ev_hndl (struct msgbuf_head *bh)
{
	struct sockbase *sb = cont_of (bh, struct sockbase, rcv);
	struct ipc_sock *ipcsk = cont_of (sb, struct ipc_sock, base);
	struct worker *cpu = get_worker (sb->cpu_no);

	/* Enable POLLOUT event when snd_head isn't empty */
	if (! (ipcsk->et.events & EPOLLIN) ) {
		DEBUG_OFF ("%d enable EPOLLIN", sb->fd);
		ipcsk->et.events |= EPOLLIN;
		BUG_ON (eloop_mod (&cpu->el, &ipcsk->et) != 0);
	}
}

int ipc_connector_hndl (eloop_t *el, ev_t *et);

struct sockbase *ipc_alloc()
{
	struct ipc_sock *ipcsk = TNEW (struct ipc_sock);

	if (!ipcsk)
		return 0;
	sockbase_init (&ipcsk->base);
	bio_init (&ipcsk->in);
	bio_init (&ipcsk->out);
	INIT_LIST_HEAD (&ipcsk->sg_head);
	return &ipcsk->base;
}

static int ipc_send (struct sockbase *sb, char *ubuf)
{
	int rc;
	struct msgbuf *msg = cont_of (ubuf, struct msgbuf, chunk.ubuf_base);

	if ((rc = snd_msgbuf_head_add (sb, msg)) < 0)
		errno = sb->fepipe ? EPIPE : EAGAIN;
	return rc;
}

int ipc_socket_init (struct sockbase *sb, int sys_fd)
{
	struct ipc_sock *ipcsk = cont_of (sb, struct ipc_sock, base);
	struct worker *cpu = get_worker (sb->cpu_no);
	int on = 1;

	BUG_ON (!cpu);
	ipcsk->vtp = tp_get (sb->vfptr->pf);
	ipcsk->sys_fd = sys_fd;
	ipcsk->vtp->setopt (sys_fd, TP_NOBLOCK, &on, sizeof (on));
	ipcsk->vtp->setopt (sys_fd, TP_SNDBUF, &default_sndbuf, sizeof (default_sndbuf));
	ipcsk->vtp->setopt (sys_fd, TP_RCVBUF, &default_rcvbuf, sizeof (default_rcvbuf));

	ipcsk->ops = ipc_io_ops;
	ipcsk->et.events = EPOLLIN|EPOLLRDHUP|EPOLLERR|EPOLLHUP;
	ipcsk->et.fd = sys_fd;
	ipcsk->et.f = ipc_connector_hndl;
	ipcsk->et.data = ipcsk;
	BUG_ON (eloop_add (&cpu->el, &ipcsk->et) != 0);

	msgbuf_head_handle_rm (&sb->rcv, rcv_msgbuf_head_rm_ev_hndl);
	msgbuf_head_handle_full (&sb->rcv, rcv_msgbuf_head_full_ev_hndl);
	msgbuf_head_handle_nonfull (&sb->rcv, rcv_msgbuf_head_nonfull_ev_hndl);
	msgbuf_head_handle_empty (&sb->snd, snd_msgbuf_head_empty_ev_hndl);
	msgbuf_head_handle_nonempty (&sb->snd, snd_msgbuf_head_nonempty_ev_hndl);
	return 0;
}

static int ipc_connector_bind (struct sockbase *sb, const char *sock)
{
	struct ipc_sock *ipcsk = cont_of (sb, struct ipc_sock, base);
	struct worker *cpu = get_worker (sb->cpu_no);
	int sys_fd = 0;

	BUG_ON (!cpu);
	ipcsk->vtp = tp_get (sb->vfptr->pf);
	if ((sys_fd = ipcsk->vtp->connect (sock)) < 0)
		return -1;
	strcpy (sb->peer, sock);
	return ipc_socket_init (sb, sys_fd);
}

static int ipc_connector_snd (struct sockbase *sb);

static void ipc_connector_close (struct sockbase *sb)
{
	struct ipc_sock *ipcsk = cont_of (sb, struct ipc_sock, base);
	struct worker *cpu = get_worker (sb->cpu_no);
	
	BUG_ON (!ipcsk->vtp);

	/* Try flush buf massage into network before close */
	ipc_connector_snd (sb);

	/* Detach sock low-level file descriptor from poller */
	if (ipcsk->sys_fd > 0) {
		BUG_ON (eloop_del (&cpu->el, &ipcsk->et) != 0);
		ipcsk->vtp->close (ipcsk->sys_fd);
	}

	ipcsk->sys_fd = -1;
	ipcsk->et.events = -1;
	ipcsk->et.fd = -1;
	ipcsk->et.f = 0;
	ipcsk->et.data = 0;
	ipcsk->vtp = 0;

	/* Destroy the sock base and free sockid. */
	sockbase_exit (sb);
	mem_free (ipcsk, sizeof (*ipcsk) );
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

static int ipc_connector_rcv (struct sockbase *sb)
{
	struct ipc_sock *ipcsk = cont_of (sb, struct ipc_sock, base);
	int rc = 0;
	u16 cmsg_num;
	struct msgbuf *aim = 0, *cmsg = 0;

	rc = bio_prefetch (&ipcsk->in, &ipcsk->ops);
	if (rc < 0 && errno != EAGAIN)
		return rc;
	while (bufio_check_msg (&ipcsk->in) ) {
		aim = 0;
		bufio_rm (&ipcsk->in, &aim);
		BUG_ON (!aim);
		cmsg_num = aim->chunk.cmsg_num;
		while (cmsg_num--) {
			cmsg = 0;
			bufio_rm (&ipcsk->in, &cmsg);
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
	struct ipc_sock *ipcsk = cont_of (sb, struct ipc_sock, base);
	int rc;
	struct iovec *iov;
	struct msgbuf *msg;
	struct msghdr msghdr = {};

	/* First. sending the bufio caching */
	if (bio_size (&ipcsk->out) > 0) {
		if ( (rc = bio_flush (&ipcsk->out, &ipcsk->ops) ) < 0)
			return rc;
		if (bio_size (&ipcsk->out) > 0) {
			errno = EAGAIN;
			return -1;
		}
	}
	/* Second. sending the scatter-gather iovec */
	BUG_ON (bio_size (&ipcsk->out) );
	if (!ipcsk->iov_length)
		return 0;
	msghdr.msg_iov = ipcsk->biov + ipcsk->iov_start;
	msghdr.msg_iovlen = ipcsk->iov_end - ipcsk->iov_start;
	if ( (rc = ipcsk->vtp->sendmsg (ipcsk->sys_fd, &msghdr, 0) ) < 0)
		return rc;
	iov = &ipcsk->biov[ipcsk->iov_start];
	while (rc >= iov->iov_len && iov < &ipcsk->biov[ipcsk->iov_end]) {
		rc -= iov->iov_len;
		msg = cont_of (iov->iov_base, struct msgbuf, chunk);
		list_del_init (&msg->item);
		msgbuf_free (msg);
		iov++;
	}
	/* Cache the reset iovec into bufio  */
	if (rc > 0) {
		bio_write (&ipcsk->out, iov->iov_base + rc, iov->iov_len - rc);
		msg = cont_of (iov->iov_base, struct msgbuf, chunk);
		list_del_init (&msg->item);
		msgbuf_free (msg);
		rc = 0;
		iov++;
	}
	ipcsk->iov_start = iov - ipcsk->biov;
	BUG_ON (iov > &ipcsk->biov[ipcsk->iov_end]);
	if (bio_size (&ipcsk->out) || ipcsk->iov_start < ipcsk->iov_end) {
		errno = EAGAIN;
		return -1;
	}
	if (ipcsk->iov_length > NELEM (ipcsk->iov, struct iovec) )
		mem_free (ipcsk->biov, ipcsk->iov_length * sizeof (struct iovec) );
	ipcsk->biov = 0;
	ipcsk->iov_start = ipcsk->iov_end = ipcsk->iov_length = 0;
	return 0;
}

static int ipc_connector_sg (struct sockbase *sb)
{
	struct ipc_sock *ipcsk = cont_of (sb, struct ipc_sock, base);
	int rc;
	struct msgbuf *msg, *nmsg;
	struct iovec *iov;

	while ( (rc = sg_send (sb) ) == 0) {
		BUG_ON (!list_empty (&ipcsk->sg_head) );

		/* Third. serialize the queue message for send */
		while ( (msg = snd_msgbuf_head_rm (sb) ) )
			ipcsk->iov_length += msgbuf_serialize (msg, &ipcsk->sg_head);
		if (ipcsk->iov_length <= 0) {
			errno = EAGAIN;
			return -1;
		}
		ipcsk->iov_end = ipcsk->iov_length;
		if (ipcsk->iov_length <= NELEM (ipcsk->iov, struct iovec) ) {
			ipcsk->biov = &ipcsk->iov[0];
		} else {
			/* BUG here ? */
			ipcsk->biov = NTNEW (struct iovec, ipcsk->iov_length);
			BUG_ON (!ipcsk->biov);
		}
		iov = ipcsk->biov;
		walk_msg_s (msg, nmsg, &ipcsk->sg_head) {
			list_del_init (&msg->item);
			iov->iov_base = msgbuf_base (msg);
			iov->iov_len = msgbuf_len (msg);
			iov++;
		}
	}
	return rc;
}

static int ipc_connector_snd (struct sockbase *sb)
{
	struct ipc_sock *ipcsk = cont_of (sb, struct ipc_sock, base);
	int rc;
	struct msgbuf *msg;

	if (ipcsk->vtp->sendmsg)
		return ipc_connector_sg (sb);
	while ( (msg = snd_msgbuf_head_rm (sb) ) )
		bufio_add (&ipcsk->out, msg);
	rc = bio_flush (&ipcsk->out, &ipcsk->ops);
	return rc;
}

int ipc_connector_hndl (eloop_t *el, ev_t *et)
{
	int rc = 0;
	struct ipc_sock *ipcsk = cont_of (et, struct ipc_sock, et);
	struct sockbase *sb = &ipcsk->base;

	if (et->happened & EPOLLIN) {
		DEBUG_OFF ("io sock %d EPOLLIN", sb->fd);
		rc = ipc_connector_rcv (sb);
	}
	if (et->happened & EPOLLOUT) {
		DEBUG_OFF ("io sock %d EPOLLOUT", sb->fd);
		rc = ipc_connector_snd (sb);
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

struct sockbase_vfptr ipc_connector_vfptr = {
	.type = XCONNECTOR,
	.pf = TP_IPC,
	.alloc = ipc_alloc,
	.send = ipc_send,
	.bind = ipc_connector_bind,
	.close = ipc_connector_close,
	.setopt = 0,
	.getopt = 0,
};
