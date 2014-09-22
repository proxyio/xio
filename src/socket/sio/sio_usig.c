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
#include "sio.h"


enum {
	/* Following msgbuf_head events are provided by sockbase */
	EV_SNDBUF_ADD        =     0x001,
	EV_SNDBUF_RM         =     0x002,
	EV_SNDBUF_EMPTY      =     0x004,
	EV_SNDBUF_NONEMPTY   =     0x008,
	EV_SNDBUF_FULL       =     0x010,
	EV_SNDBUF_NONFULL    =     0x020,

	EV_RCVBUF_ADD        =     0x101,
	EV_RCVBUF_RM         =     0x102,
	EV_RCVBUF_EMPTY      =     0x104,
	EV_RCVBUF_NONEMPTY   =     0x108,
	EV_RCVBUF_FULL       =     0x110,
	EV_RCVBUF_NONFULL    =     0x120,
};


static void snd_msgbuf_head_empty_ev_hndl (struct sockbase *sb)
{
	struct sio *tcps = cont_of (sb, struct sio, base);

	mutex_lock (&sb->lock);

	if (msgbuf_head_empty(&sb->snd) && (tcps->et.events & EV_WRITE)) {
		SKLOG_DEBUG (sb, "%d disable EV_WRITE", sb->fd);
		tcps->et.events &= ~EV_WRITE;
		BUG_ON (__ev_fdset_ctl (&tcps->el->fdset, EV_MOD, &tcps->et) != 0);
		socket_mstats_incr (&sb->stats, ST_EV_WRITE_OFF);
	}
	mutex_unlock (&sb->lock);
}

static void snd_msgbuf_head_nonempty_ev_hndl (struct sockbase *sb)
{
	struct sio *tcps = cont_of (sb, struct sio, base);

	mutex_lock (&sb->lock);
	/* Enable POLLOUT event when snd_head isn't empty */
	if (!(tcps->et.events & EV_WRITE)) {
		SKLOG_DEBUG (sb, "%d enable EV_WRITE", sb->fd);
		tcps->et.events |= EV_WRITE;
		BUG_ON (__ev_fdset_ctl (&tcps->el->fdset, EV_MOD, &tcps->et) != 0);
		socket_mstats_incr (&sb->stats, ST_EV_WRITE_ON);
	}
	mutex_unlock (&sb->lock);
}

static void rcv_msgbuf_head_full_ev_hndl (struct sockbase *sb)
{
	struct sio *tcps = cont_of (sb, struct sio, base);

	mutex_lock (&sb->lock);
	/* Enable POLLOUT event when snd_head isn't empty */
	if ((tcps->et.events & EV_READ)) {
		SKLOG_DEBUG (sb, "%d disable EV_READ", sb->fd);
		tcps->et.events &= ~EV_READ;
		BUG_ON (__ev_fdset_ctl (&tcps->el->fdset, EV_MOD, &tcps->et) != 0);
		socket_mstats_incr (&sb->stats, ST_EV_READ_OFF);
	}
	mutex_unlock (&sb->lock);
}


static void rcv_msgbuf_head_nonfull_ev_hndl (struct sockbase *sb)
{
	struct sio *tcps = cont_of (sb, struct sio, base);

	mutex_lock (&sb->lock);
	/* Enable POLLOUT event when snd_head isn't empty */
	if (!(tcps->et.events & EV_READ)) {
		SKLOG_DEBUG (sb, "%d enable EV_READ", sb->fd);
		tcps->et.events |= EV_READ;
		BUG_ON (__ev_fdset_ctl (&tcps->el->fdset, EV_MOD, &tcps->et) != 0);
		socket_mstats_incr (&sb->stats, ST_EV_READ_ON);
	}
	mutex_unlock (&sb->lock);
}




static void sndhead_empty_ev_usignal (struct msgbuf_head *bh)
{
	struct sio *tcps = cont_of (bh, struct sio, base.snd);
	ev_signal (&tcps->sig, EV_SNDBUF_EMPTY);
}

static void sndhead_nonempty_ev_usignal (struct msgbuf_head *bh)
{
	struct sio *tcps = cont_of (bh, struct sio, base.snd);
	ev_signal (&tcps->sig, EV_SNDBUF_NONEMPTY);
}

static void rcvhead_full_ev_usignal (struct msgbuf_head *bh)
{
	struct sio *tcps = cont_of (bh, struct sio, base.rcv);
	ev_signal (&tcps->sig, EV_RCVBUF_FULL);
}

static void rcvhead_nonfull_ev_usignal (struct msgbuf_head *bh)
{
	struct sio *tcps = cont_of (bh, struct sio, base.rcv);
	ev_signal (&tcps->sig, EV_RCVBUF_NONFULL);
}

static struct msgbuf_vfptr tcps_sndhead_vf = {
	.empty = sndhead_empty_ev_usignal,
	.nonempty = sndhead_nonempty_ev_usignal,
};
struct msgbuf_vfptr *tcps_sndhead_vfptr = &tcps_sndhead_vf;


static struct msgbuf_vfptr tcps_rcvhead_vf = {
	.full = rcvhead_full_ev_usignal,
	.nonfull = rcvhead_nonfull_ev_usignal,
};
struct msgbuf_vfptr *tcps_rcvhead_vfptr = &tcps_rcvhead_vf;


void sio_usignal_hndl (struct ev_sig *sig, int ev)
{
	struct sio *tcps = cont_of (sig, struct sio, sig);
	switch (ev) {
		case EV_SNDBUF_EMPTY:
			snd_msgbuf_head_empty_ev_hndl (&tcps->base);
			break;
		case EV_SNDBUF_NONEMPTY:
			snd_msgbuf_head_nonempty_ev_hndl (&tcps->base);
			break;
		case EV_RCVBUF_FULL:
			rcv_msgbuf_head_full_ev_hndl (&tcps->base);
			break;
		case EV_RCVBUF_NONFULL:
			rcv_msgbuf_head_nonfull_ev_hndl (&tcps->base);
			break;
	}
}


