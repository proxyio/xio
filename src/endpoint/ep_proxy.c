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
#include <xio/socket.h>
#include <xio/poll.h>
#include "ep_struct.h"

extern int __xep_send(struct endpoint *ep, char *ubuf, struct endsock **go);

static void producer_event_hndl(struct endsock *sk) {
    struct endpoint *ep = sk->owner;
    struct proxy *py = ep->owner;
    int rc;
    char *ubuf = 0;
    struct endsock *dst = 0;
    
    BUG_ON(ep->type != XEP_PRODUCER);
    if ((rc = recv_vfptr[ep->type] (sk, &ubuf)) == 0) {
	rc = __xep_send(py->frontend, ubuf, &dst);
	DEBUG_OFF("ep(%d)'s dispatcher %d route resp: %10.10s backto %d", ep2eid(ep),
		  sk->fd, ubuf, dst ? dst->fd : -1);
	if (rc < 0)
	    xep_freeubuf(ubuf);
    } else if (errno != EAGAIN)
	DEBUG_OFF("ep(%d)'s dispatcher %d bad status of errno %d", ep2eid(ep),
		  sk->fd, errno);
}

static void comsumer_event_hndl(struct endsock *sk) {
    struct endpoint *ep = sk->owner;
    struct proxy *py = ep->owner;
    int rc;
    char *ubuf;
    struct endsock *dst = 0;
    
    BUG_ON(ep->type != XEP_COMSUMER);
    if ((rc = recv_vfptr[ep->type] (sk, &ubuf)) == 0) {

	BUG_ON(ep != py->frontend);
	rc = __xep_send(py->backend, ubuf, &dst);
	if (dst)
	    BUG_ON(dst->owner != py->backend);

	DEBUG_OFF("ep(%d)'s receiver %d send req: %10.10s goto %d", ep2eid(ep),
		  sk->fd, ubuf, dst ? dst->fd : -1);
	if (rc < 0)
	    xep_freeubuf(ubuf);
    } else if (errno != EAGAIN)
	DEBUG_OFF("ep(%d)'s receiver %d bad status of errno %d", ep2eid(ep),
		  sk->fd, errno);
}


static void connector_event_hndl(struct endsock *sk) {
    struct endpoint *ep = sk->owner;

    switch (ep->type) {
    case /* dispatcher */
	XEP_PRODUCER:
	producer_event_hndl(sk);
	break;
    case /* receiver */
	XEP_COMSUMER:
	comsumer_event_hndl(sk);
	break;
    default:
	BUG_ON(1);
    }
    /* TODO: bad status socket */
}

extern const char *xpoll_str[];
static int sk_enable_poll(struct xpoll_t *po, struct endsock *sk);

static void listener_event_hndl(struct endsock *sk) {
    struct endpoint *ep = sk->owner;
    struct proxy *py = ep->owner;
    struct endsock *newsk;

    DEBUG_OFF("ep(%d)'s listener %d events %s", ep2eid(ep), sk->fd,
	      xpoll_str[sk->ent.happened]);
    while ((newsk = endpoint_accept(ep2eid(ep), sk))) {
	BUG_ON(sk_enable_poll(py->po, newsk));
	DEBUG_OFF("ep(%d)'s listener %d accept %d", ep2eid(ep), sk->fd,
		  newsk->fd);
    }
}

static void event_hndl(struct xpoll_event *ent) {
    int socktype = 0;
    int optlen;
    struct endsock *sk = (struct endsock *)ent->self;

    sk->ent.happened = ent->happened;
    xgetopt(sk->fd, XL_SOCKET, XSOCKTYPE, &socktype, &optlen);
    switch (socktype) {
    case XCONNECTOR:
	connector_event_hndl(sk);
	break;
    case XLISTENER:
	listener_event_hndl(sk);
	break;
    default:
	BUG_ON(1);
    }
};


static int sk_enable_poll(struct xpoll_t *po, struct endsock *sk) {
    int rc;
    sk->ent.xd = sk->fd;
    sk->ent.self = sk;
    sk->ent.care = XPOLLIN|XPOLLERR;
    BUG_ON((rc = xpoll_ctl(po, XPOLL_ADD, &sk->ent)) != 0);
    return rc;
}


static void enable_sockets_poll(struct endpoint *ep) {
    struct proxy *py = ep->owner;
    struct endsock *sk, *next_sk;

    xendpoint_walk_sock(sk, next_sk, &ep->listeners) {
	BUG_ON(sk_enable_poll(py->po, sk));
	DEBUG_OFF("ep(%d)'s xpoll add listener %d", ep2eid(ep), sk->fd);
    }
    xendpoint_walk_sock(sk, next_sk, &ep->connectors) {
	BUG_ON(sk_enable_poll(py->po, sk));
	DEBUG_OFF("ep(%d)'s xpoll add connector %d", ep2eid(ep), sk->fd);
    }
}

static int py_routine(void *args) {
    int i, rc;
    struct proxy *py = (struct proxy *)args;
    struct xpoll_event ent[100];

    enable_sockets_poll(py->frontend);
    enable_sockets_poll(py->backend);

    while (!py->exiting) {
	if ((rc = xpoll_wait(py->po, ent, NELEM(ent, struct xpoll_event),
			     1)) < 0) {
	    usleep(10000);
	    continue;
	}
	DEBUG_OFF("%d sockets happened events", rc);
	for (i = 0; i < rc; i++) {
	    DEBUG_OFF("socket %d with events %s", ent[i].xd,
		      xpoll_str[ent[i].happened]);
	    event_hndl(&ent[i]);
	}
    }
    return 0;
}

struct proxy *proxy_open(int front_eid, int backend_eid) {
    struct proxy *py;

    if (!(py = proxy_new()))
	return 0;
    DEBUG_OFF("front endpoint %d and back endpoint %d", front_eid, backend_eid);
    py->exiting = false;
    spin_init(&py->lock);
    py->frontend = eid_get(front_eid);
    py->backend = eid_get(backend_eid);
    py->frontend->owner = py->backend->owner = py;
    thread_start(&py->py_worker, py_routine, py);
    return py;
}

void proxy_close(struct proxy *py) {
    struct endpoint *ep;

    BUG_ON(!py);
    py->exiting = true;
    thread_stop(&py->py_worker);

    BUG_ON(!py->backend);
    BUG_ON(!py->frontend);

    ep = py->backend;
    ep->owner = 0;
    xep_close(ep2eid(ep));

    ep = py->frontend;
    ep->owner = 0;
    xep_close(ep2eid(ep));

    proxy_free(py);
}
