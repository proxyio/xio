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
#include <runner/taskpool.h>
#include "../xgb.h"

extern struct sockbase *find_listener(const char *addr);

/******************************************************************************
 *  snd_head events trigger.
 ******************************************************************************/

static int snd_head_push(struct sockbase *sb) {
    int rc = 0;
    int can = false;
    struct xmsg *msg;
    struct sockbase *peer = (cont_of(sb, struct inproc_sock, base))->peer;

    // TODO: maybe the peer xsock can't recv anymore after the check.
    mutex_lock(&peer->lock);
    if (can_recv(peer))
	can = true;
    mutex_unlock(&peer->lock);
    if (!can)
	return -1;

    mutex_unlock(&sb->lock);
    if ((msg = sendq_pop(sb)))
	recvq_push(peer, msg);
    mutex_lock(&sb->lock);
    return rc;
}

/******************************************************************************
 *  rcv_head events trigger.
 ******************************************************************************/

static int rcv_head_pop(struct sockbase *sb) {
    int rc = 0;

    if (sb->snd.waiters)
	condition_signal(&sb->cond);
    return rc;
}


static struct sockbase *xinp_alloc() {
    struct inproc_sock *self = (struct inproc_sock *)mem_zalloc(sizeof(*self));

    if (self) {
	xsock_init(&self->base);
	return &self->base;
    }
    return 0;
}



/******************************************************************************
 *  xsock_inproc_spec
 ******************************************************************************/

static int xinp_connector_bind(struct sockbase *sb, const char *sock) {
    int nfd = 0;
    struct sockbase *nsb = 0;
    struct sockbase *listener = find_listener(sock);
    struct inproc_sock *self = 0;
    struct inproc_sock *peer = 0;
    
    if (!listener) {
	errno = ENOENT;	
	return -1;
    }
    if ((nfd = xalloc(sb->vfptr->pf, sb->vfptr->type)) < 0) {
	errno = EMFILE;
	return -1;
    }
    nsb = xgb.sockbases[nfd];
    self = cont_of(sb, struct inproc_sock, base);
    peer = cont_of(nsb, struct inproc_sock, base);
    nsb->vfptr = sb->vfptr;
    strncpy(sb->peer, sock, TP_SOCKADDRLEN);
    strncpy(nsb->addr, sock, TP_SOCKADDRLEN);

    atomic_inc(&sb->ref);
    atomic_inc(&nsb->ref);
    self->peer = &peer->base;
    peer->peer = &self->base;

    if (acceptq_push(listener, nsb) < 0) {
	errno = ECONNREFUSED;
	atomic_dec(&sb->ref);
	atomic_dec(&nsb->ref);
	xput(nfd);
	return -1;
    }
    return 0;
}

static void xinp_connector_close(struct sockbase *sb) {
    struct inproc_sock *self = cont_of(sb, struct inproc_sock, base);
    struct sockbase *peer = self->peer;

    /* Destroy the xsock and free xsock id if i hold the last ref. */
    xput(peer->fd);
    xput(sb->fd);
}


static void snd_head_notify(struct sockbase *sb, u32 events) {
    if (events & XMQ_PUSH)
	snd_head_push(sb);
}

static void rcv_head_notify(struct sockbase *sb, u32 events) {
    if (events & XMQ_POP)
	rcv_head_pop(sb);
}

static void xinp_connector_notify(struct sockbase *sb, int type, u32 events) {
    switch (type) {
    case RECV_Q:
	rcv_head_notify(sb, events);
	break;
    case SEND_Q:
	snd_head_notify(sb, events);
	break;
    default:
	BUG_ON(1);
    }
}

struct sockbase_vfptr xinp_connector_spec = {
    .type = XCONNECTOR,
    .pf = XPF_INPROC,
    .alloc = xinp_alloc,
    .bind = xinp_connector_bind,
    .close = xinp_connector_close,
    .notify = xinp_connector_notify,
    .getopt = 0,
    .setopt = 0,
};
