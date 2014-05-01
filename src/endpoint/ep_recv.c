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

static struct endsock *available_csock(struct endpoint *ep) {
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

static int generic_recv(struct endpoint *ep,
			char **ubuf, struct endsock **s) {
    int rc;
    struct ephdr *eh;
    struct endsock *at_sock;

    if (!(at_sock = available_csock(ep))) {
	errno = EAGAIN;
	return -1;
    }
    if ((rc = xrecv(at_sock->sockfd, (char **)&eh)) < 0) {
	if (errno != EAGAIN) {
	    errno = EPIPE;
	    DEBUG_OFF("socket %d EPIPE", at_sock->sockfd);
	    list_move_tail(&at_sock->link, &ep->bad_socks);
	}
	if (list_empty(&ep->bsocks) && list_empty(&ep->csocks))
	    errno = EBADF;
    } else {
	*s = at_sock;
	*ubuf = ephdr2ubuf(eh);
    }
    return rc;
}

static int producer_recv(struct endpoint *ep, char **ubuf) {
    int rc;
    struct ephdr *eh;
    struct epr *rt;
    struct endsock *sock;
    
    if ((rc = generic_recv(ep, ubuf, &sock)) == 0) {
	eh = ubuf2ephdr(*ubuf);
	rt = rt_cur(eh);
	eh->ttl--;
    }
    return rc;
}

static int comsumer_recv(struct endpoint *ep, char **ubuf) {
    int rc;
    struct ephdr *eh;
    struct epr *rt;
    struct endsock *sock;
    
    if ((rc = generic_recv(ep, ubuf, &sock)) == 0) {
	eh = ubuf2ephdr(*ubuf);
	rt = rt_cur(eh);
	if (memcmp(rt->uuid, sock->uuid, sizeof(sock->uuid)) != 0)
	    uuid_copy(sock->uuid, rt->uuid);
    }
    return rc;
}

typedef int (*rcvfunc) (struct endpoint *ep, char **ubuf);
const static rcvfunc recv_vfptr[] = {
    0,
    producer_recv,
    comsumer_recv,
};


int xep_recv(int eid, char **ubuf) {
    int rc;
    struct endpoint *ep = eid_get(eid);

    if (!(ep->type & (XEP_PRODUCER|XEP_COMSUMER))) {
	errno = EBADF;
	return -1;
    }
    accept_endsocks(eid);
    rc = recv_vfptr[ep->type] (ep, ubuf);
    return rc;
}

