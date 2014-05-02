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


static struct endsock *rrbin_forward(struct endpoint *ep, char *ubuf) {
    struct endsock *es, *next_es;
    int tmp;
    
    xendpoint_walk_sock(es, next_es, &ep->csocks) {
	if (xselect(XPOLLOUT|XPOLLERR, 1, &es->sockfd, 1, &tmp) == 0)
	    continue;
	BUG_ON(es->sockfd != tmp);
	list_move_tail(&es->link, &ep->csocks);
	return es;
    }
    errno = ENXIO;
    return 0;
}

static struct endsock *route_backward(struct endpoint *ep, char *ubuf) {
    struct endsock *es, *next_es;
    struct ephdr *eh = ubuf2ephdr(ubuf);
    struct epr *rt = rt_cur(eh);

    xendpoint_walk_sock(es, next_es, &ep->csocks) {
	if (memcmp(es->uuid, rt->uuid, sizeof(es->uuid)) != 0)
	    continue;
	return es;
    }
    errno = EAGAIN;
    return 0;
}

const target_algo send_target_vfptr[] = {
    0,
    rrbin_forward,
    route_backward,
};

static int producer_send(struct endsock *sk, char *ubuf) {
    struct endpoint *ep = sk->owner;
    int rc;
    struct ephdr *eh = ubuf2ephdr(ubuf);
    struct epr *rt = rt_cur(eh);

    eh->go = 1;
    uuid_copy(rt->uuid, sk->uuid);
    if ((rc = xsend(sk->sockfd, (char *)eh)) < 0) {
	if (errno != EAGAIN) {
	    errno = EPIPE;
	    list_move_tail(&sk->link, &ep->bad_socks);
	}
	if (list_empty(&ep->bsocks) && list_empty(&ep->csocks))
	    errno = EBADF;
    }
    return rc;
}

static int comsumer_send(struct endsock *sk, char *ubuf) {
    struct endpoint *ep = sk->owner;
    int rc;
    struct ephdr *eh = ubuf2ephdr(ubuf);

    eh->go = 0;
    if (!eh->end_ttl)
	eh->end_ttl = eh->ttl;
    if ((rc = xsend(sk->sockfd, (char *)eh)) < 0) {
	if (errno != EAGAIN) {
	    errno = EPIPE;
	    list_move_tail(&sk->link, &ep->bad_socks);
	}
	if (list_empty(&ep->bsocks) && list_empty(&ep->csocks))
	    errno = EBADF;
    }
    DEBUG_OFF("%d send with rc %d", sk->sockfd, rc);
    return rc;
}

const send_action send_vfptr[] = {
    0,
    producer_send,
    comsumer_send,
};

int __xep_send(struct endpoint *ep, char *ubuf) {
    int rc;
    struct endsock *sk;

    if (!(sk = send_target_vfptr[ep->type] (ep, ubuf)))
	return -1;
    rc = send_vfptr[ep->type] (sk, ubuf);
    return rc;
}

int xep_send(int eid, char *ubuf) {
    struct endpoint *ep = eid_get(eid);

    if (!(ep->type & (XEP_PRODUCER|XEP_COMSUMER))) {
	errno = EBADF;
	return -1;
    }
    accept_endsocks(eid);
    return __xep_send(ep, ubuf);
}
