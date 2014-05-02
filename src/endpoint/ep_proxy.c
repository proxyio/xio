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


static int sk_xpoll_enable(struct xeppy *py, struct endsock *sk) {
    int rc;
    sk->ent.xd = sk->sockfd;
    sk->ent.self = sk;
    sk->ent.care = XPOLLIN|XPOLLOUT|XPOLLERR;
    rc = xpoll_ctl(py->po, XPOLL_ADD, &sk->ent);
    return 0;
}


extern int __xep_send(struct endpoint *ep, char *ubuf);

static void producer_event_hndl(struct endsock *sk) {
    struct endpoint *ep = sk->owner;
    struct xeppy *py = ep->owner;
    int rc;
    char *ubuf = 0;

    if ((rc = recv_vfptr[XEP_PRODUCER] (sk, &ubuf)) == 0) {
	if (__xep_send(py->frontend, ubuf) < 0 && errno != EAGAIN) {
	    xep_freeubuf(ubuf);
	    DEBUG_ON("ubuf route back with errno %d", errno);
	}
    }
}

static void comsumer_event_hndl(struct endsock *sk) {
    struct endpoint *ep = sk->owner;
    struct xeppy *py = ep->owner;
    int rc;
    char *ubuf, *ubuf2;
    struct uhdr *uh;
    struct ephdr *eh;
    
    if ((rc = recv_vfptr[XEP_COMSUMER] (sk, &ubuf)) == 0) {
	ubuf2 = xep_allocubuf(XEPUBUF_CLONEHDR, xep_ubuflen(ubuf), ubuf);
	BUG_ON(!ubuf2);
	memcpy(ubuf2, ubuf, xep_ubuflen(ubuf));
	xep_freeubuf(ubuf);

	eh = ubuf2ephdr(ubuf2);
	eh->ttl++;
	uh = ephdr_uhdr(eh);
	uh->ephdr_off = ephdr_ctlen(eh);

	if (__xep_send(py->backend, ubuf2) < 0 && errno != EAGAIN) {
	    xep_freeubuf(ubuf2);
	    DEBUG_ON("ubuf dispatcher with errno %d", errno);
	}
    }
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

extern void endpoint_accept(int eid, struct endsock *sk);

static void listener_event_hndl(struct endsock *sk) {
    struct endpoint *ep = sk->owner;
    endpoint_accept(ep2eid(ep), sk);
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
	for (i = 0; i < rc; i++) {
	    event_hndl(&ent[i]);
	}
    }
    return 0;
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
