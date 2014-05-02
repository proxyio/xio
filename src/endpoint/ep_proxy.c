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

extern int __xep_send(struct endpoint *ep, char *ubuf);

static void producer_event_hndl(struct endsock *sk) {
    struct endpoint *ep = sk->owner;
    struct xeppy *py = ep->owner;
    int rc;
    char *ubuf = 0;

    BUG_ON(ep->type != XEP_PRODUCER);
    if ((rc = recv_vfptr[ep->type] (sk, &ubuf)) == 0) {
	if (__xep_send(py->frontend, ubuf) < 0 && errno != EAGAIN) {
	    xep_freeubuf(ubuf);
	    DEBUG_ON("ubuf route back with errno %d", errno);
	}
	DEBUG_OFF("dispatcher %d recv response", sk->sockfd);
    } else if (errno != EAGAIN)
	DEBUG_OFF("dispatcher %d bad status of errno %d", sk->sockfd, errno);
}

static void comsumer_event_hndl(struct endsock *sk) {
    struct endpoint *ep = sk->owner;
    struct xeppy *py = ep->owner;
    int rc;
    char *ubuf, *ubuf2;

    BUG_ON(ep->type != XEP_COMSUMER);
    if ((rc = recv_vfptr[ep->type] (sk, &ubuf)) == 0) {
	ubuf2 = xep_allocubuf(XEPUBUF_CLONEHDR|XEPUBUF_APPENDRT,
			      xep_ubuflen(ubuf), ubuf);
	BUG_ON(!ubuf2);
	memcpy(ubuf2, ubuf, xep_ubuflen(ubuf));
	xep_freeubuf(ubuf);

	if (__xep_send(py->backend, ubuf2) < 0 && errno != EAGAIN) {
	    xep_freeubuf(ubuf2);
	    DEBUG_ON("ubuf dispatcher with errno %d", errno);
	}
	DEBUG_OFF("receiver %d recv request", sk->sockfd);
    } else if (errno != EAGAIN)
	DEBUG_OFF("receiver %d bad status of errno %d", sk->sockfd, errno);
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
    struct xeppy *py = ep->owner;
    struct endsock *newsk;

    DEBUG_OFF("socket %d events %s", sk->sockfd, xpoll_str[sk->ent.happened]);
    if ((newsk = endpoint_accept(ep2eid(ep), sk))) {
	BUG_ON(sk_enable_poll(py->po, newsk));
    }
}

static void event_hndl(struct xpoll_event *ent) {
    int socktype = 0;
    int optlen;
    struct endsock *sk = (struct endsock *)ent->self;

    sk->ent.happened = ent->happened;
    xgetsockopt(sk->sockfd, XL_SOCKET, XSOCKTYPE, &socktype, &optlen);
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

static int py_routine(void *args) {
    int i, rc;
    struct xeppy *py = (struct xeppy *)args;
    struct xpoll_event ent[100];

    while (!py->exiting) {
	if ((rc = xpoll_wait(py->po, ent, NELEM(ent, struct xpoll_event),
			     1)) < 0) {
	    usleep(10000);
	    return -1;
	}
	DEBUG_OFF("%d sockets happened events", rc);
	for (i = 0; i < rc; i++) {
	    DEBUG_ON("socket %d with events %s", ent[i].xd,
		     xpoll_str[ent[i].happened]);
	    event_hndl(&ent[i]);
	}
    }
    return 0;
}



static int sk_enable_poll(struct xpoll_t *po, struct endsock *sk) {
    int rc;
    sk->ent.xd = sk->sockfd;
    sk->ent.self = sk;
    sk->ent.care = XPOLLIN|XPOLLERR;
    rc = xpoll_ctl(po, XPOLL_ADD, &sk->ent);
    return rc;
}


static void enable_sockets_poll(struct endpoint *ep) {
    struct xeppy *py = ep->owner;
    struct endsock *sk, *next_sk;

    xendpoint_walk_sock(sk, next_sk, &ep->bsocks) {
	BUG_ON(sk_enable_poll(py->po, sk));
    }
    xendpoint_walk_sock(sk, next_sk, &ep->csocks) {
	BUG_ON(sk_enable_poll(py->po, sk));
    }
}

struct xeppy *xeppy_open(int front_eid, int backend_eid) {
    struct xeppy *py = (struct xeppy *)mem_zalloc(sizeof(*py));

    if (!py)
	return 0;
    if (!(py->po = xpoll_create())) {
	mem_free(py, sizeof(*py));
	return 0;
    }
    py->exiting = false;
    spin_init(&py->lock);
    py->frontend = eid_get(front_eid);
    py->backend = eid_get(backend_eid);
    py->frontend->owner = py->backend->owner = py;
    enable_sockets_poll(py->frontend);
    enable_sockets_poll(py->backend);
    thread_start(&py->py_worker, py_routine, py);
    return py;
}

void xeppy_close(struct xeppy *py) {
    if (!py) {
	errno = EINVAL;
	return;
    }
    py->exiting = true;
    thread_stop(&py->py_worker);
    xpoll_close(py->po);
    spin_destroy(&py->lock);
}
