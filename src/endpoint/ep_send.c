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

static struct endsock *rrbin_forward(struct endpoint *ep) {
    struct endsock *es, *next_es;
    int tmp;
    
    xendpoint_walk_sock(es, next_es, &ep->csocks) {
	if (xselect(XPOLLOUT|XPOLLERR, 1, &es->sockfd, 1, &tmp) == 0)
	    continue;
	BUG_ON(es->sockfd != tmp);
	list_move_tail(&es->link, &ep->csocks);
	return es;
    }
    return 0;
}

static int producer_send(struct endpoint *ep, char *ubuf) {
    int rc;
    struct endsock *go_sock;
    struct ephdr *eh = ubuf2ephdr(ubuf);
    struct epr *rt = rt_cur(eh);

    if (!(go_sock = rrbin_forward(ep))) {
	errno = EAGAIN;
	return -1;
    }
    eh->go = 1;
    uuid_copy(rt->uuid, go_sock->uuid);
    if ((rc = xsend(go_sock->sockfd, (char *)eh)) < 0) {
	if (errno != EAGAIN) {
	    errno = EPIPE;
	    list_move_tail(&go_sock->link, &ep->bad_socks);
	}
	if (list_empty(&ep->bsocks) && list_empty(&ep->csocks))
	    errno = EBADF;
    }
    return rc;
}

static struct endsock *route_backward(struct endpoint *ep, uuid_t ud) {
    struct endsock *es, *next_es;

    xendpoint_walk_sock(es, next_es, &ep->csocks) {
	if (memcmp(es->uuid, ud, sizeof(es->uuid)) != 0)
	    continue;
	return es;
    }
    return 0;
}

static int comsumer_send(struct endpoint *ep, char *ubuf) {
    int rc;
    struct endsock *back_sock;
    struct ephdr *eh = ubuf2ephdr(ubuf);
    struct epr *rt = &eh->rt[0];

    if (!(back_sock = route_backward(ep, rt->uuid))) {
	errno = ENXIO;
	return -1;
    }
    eh->go = 0;
    eh->end_ttl = eh->ttl;
    if ((rc = xsend(back_sock->sockfd, (char *)eh)) < 0) {
	if (errno != EAGAIN) {
	    errno = EPIPE;
	    list_move_tail(&back_sock->link, &ep->bad_socks);
	}
	if (list_empty(&ep->bsocks) && list_empty(&ep->csocks))
	    errno = EBADF;
    }
    return rc;
}

typedef int (*sndfunc) (struct endpoint *ep, char *ubuf);
const static sndfunc send_vfptr[] = {
    0,
    producer_send,
    comsumer_send,
};


int xep_send(int eid, char *ubuf) {
    int rc;
    struct endpoint *ep = eid_get(eid);

    if (!(ep->type & (XEP_PRODUCER|XEP_COMSUMER))) {
	errno = EBADF;
	return -1;
    }
    accept_endsocks(eid);
    rc = send_vfptr[ep->type] (ep, ubuf);
    return rc;
}
