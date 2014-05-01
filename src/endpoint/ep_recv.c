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

static struct endsock *get_available_csock(struct endpoint *ep) {
    struct endsock *es, *next_es;
    int tmp;

    xendpoint_walk_sock(es, next_es, &ep->csocks) {
	if (xselect(XPOLLIN|XPOLLERR, 1, &es->sockfd, 1, &tmp) == 0)
	    continue;
	BUG_ON(es->sockfd != tmp);
	list_move_tail(&es->link, &ep->csocks);
	return es;
    }
    return 0;
}

static int generic_recv(struct endpoint *ep, struct endsock *sk, char **ubuf) {
    int rc;
    struct ephdr *eh;

    if ((rc = xrecv(sk->sockfd, (char **)&eh)) < 0) {
	if (errno != EAGAIN) {
	    errno = EPIPE;
	    DEBUG_OFF("socket %d epipe", sk->sockfd);
	    list_move_tail(&sk->link, &ep->bad_socks);
	}
	if (list_empty(&ep->bsocks) && list_empty(&ep->csocks))
	    errno = EBADF;
    } else {
	*ubuf = ephdr2ubuf(eh);
    }
    return rc;
}

static int producer_recv(struct endpoint *ep, struct endsock *sk, char **ubuf) {
    int rc;
    struct ephdr *eh;
    struct epr *rt;

    if ((rc = generic_recv(ep, sk, ubuf)) == 0) {
	eh = ubuf2ephdr(*ubuf);
	rt = rt_cur(eh);
	eh->ttl--;
    }
    return rc;
}

static int comsumer_recv(struct endpoint *ep, struct endsock *sk, char **ubuf) {
    int rc;
    struct ephdr *eh;
    struct epr *rt;

    if ((rc = generic_recv(ep, sk, ubuf)) == 0) {
	eh = ubuf2ephdr(*ubuf);
	rt = rt_cur(eh);
	if (memcmp(rt->uuid, sk->uuid, sizeof(sk->uuid)) != 0)
	    uuid_copy(sk->uuid, rt->uuid);
    }
    return rc;
}

const recv_action recv_vfptr[] = {
    0,
    producer_recv,
    comsumer_recv,
};

int xep_recv(int eid, char **ubuf) {
    int rc;
    struct endsock *sk;
    struct endpoint *ep = eid_get(eid);

    if (!(ep->type & (XEP_PRODUCER|XEP_COMSUMER))) {
	errno = EBADF;
	return -1;
    }
    accept_endsocks(eid);
    if (!(sk = get_available_csock(ep))) {
	errno = EAGAIN;
	return -1;
    }
    rc = recv_vfptr[ep->type] (ep, sk, ubuf);
    return rc;
}

