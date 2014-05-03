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
#include "xgb.h"

static int xinp_put(struct xsock *xsk) {
    int old;
    mutex_lock(&xsk->lock);
    old = xsk->proc.ref--;
    xsk->fok = false;
    mutex_unlock(&xsk->lock);
    return old;
}

extern struct xsock *find_listener(const char *addr);


/******************************************************************************
 *  snd_head events trigger.
 ******************************************************************************/

static int snd_head_push(int fd) {
    int rc = 0, can = false;
    struct xmsg *msg;
    struct xsock *xsk = xget(fd);
    struct xsock *peer = xsk->proc.xsock_peer;

    // Unlock myself first because i hold the lock
    mutex_unlock(&xsk->lock);

    // TODO: maybe the peer xsock can't recv anymore after the check.
    mutex_lock(&peer->lock);
    if (can_recv(peer))
	can = true;
    mutex_unlock(&peer->lock);
    if (!can)
	return -1;
    if ((msg = sendq_pop(xsk)))
	recvq_push(peer, msg);

    mutex_lock(&xsk->lock);
    return rc;
}

/******************************************************************************
 *  rcv_head events trigger.
 ******************************************************************************/

static int rcv_head_pop(int fd) {
    int rc = 0;
    struct xsock *xsk = xget(fd);

    if (xsk->snd_waiters)
	condition_signal(&xsk->cond);
    return rc;
}



/******************************************************************************
 *  xsock_inproc_protocol
 ******************************************************************************/

static int xinp_connector_bind(int fd, const char *sock) {
    struct xsock *xsk = xget(fd);
    struct xsock *req_xsk = xsock_alloc();
    struct xsock *listener = find_listener(sock);

    if (!listener) {
	errno = ENOENT;	
	return -1;
    }
    if (!req_xsk) {
	errno = EMFILE;
	return -1;
    }

    ZERO(xsk->proc);
    ZERO(req_xsk->proc);

    req_xsk->pf = xsk->pf;
    req_xsk->type = xsk->type;
    req_xsk->l4proto = xsk->l4proto;
    strncpy(xsk->peer, sock, TP_SOCKADDRLEN);
    strncpy(req_xsk->addr, sock, TP_SOCKADDRLEN);

    req_xsk->proc.ref = xsk->proc.ref = 2;
    xsk->proc.xsock_peer = req_xsk;
    req_xsk->proc.xsock_peer = xsk;

    if (acceptq_push(listener, req_xsk) < 0) {
	errno = ECONNREFUSED;
	xsock_free(req_xsk);
	return -1;
    }
    return 0;
}

static void xinp_connector_close(int fd) {
    struct xsock *xsk = xget(fd);    
    struct xsock *peer = xsk->proc.xsock_peer;

    /* Destroy the xsock and free xsock id if i hold the last ref. */
    if (xinp_put(peer) == 1) {
	xsock_free(peer);
    }
    if (xinp_put(xsk) == 1) {
	xsock_free(xsk);
    }
}


static void snd_head_notify(int fd, u32 events) {
    if (events & XMQ_PUSH)
	snd_head_push(fd);
}

static void rcv_head_notify(int fd, u32 events) {
    if (events & XMQ_POP)
	rcv_head_pop(fd);
}

static void xinp_connector_notify(int fd, int type, u32 events) {
    switch (type) {
    case RECV_Q:
	rcv_head_notify(fd, events);
	break;
    case SEND_Q:
	snd_head_notify(fd, events);
	break;
    default:
	BUG_ON(1);
    }
}

struct xsock_protocol xinp_connector_protocol = {
    .type = XCONNECTOR,
    .pf = XPF_INPROC,
    .bind = xinp_connector_bind,
    .close = xinp_connector_close,
    .notify = xinp_connector_notify,
    .getsockopt = 0,
    .setsockopt = 0,
};
