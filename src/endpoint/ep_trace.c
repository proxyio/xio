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
#include "ep_struct.h"

static void trace_listener(struct endpoint *ep) {
    struct endsock *es, *next_es;
    int sndbuf;
    int rcvbuf;
    int optlen;

    fprintf(stdout, "%10s %10s %10s\n", "listener", "rcvbuf", "sndbuf");
    fprintf(stdout, "----------------------------------------------\n");

    xendpoint_walk_sock(es, next_es, &ep->bsocks) {
	sndbuf = rcvbuf = 0;
	xgetsockopt(es->sockfd, XL_SOCKET, XSNDBUF, &sndbuf, &optlen);
	xgetsockopt(es->sockfd, XL_SOCKET, XRCVBUF, &rcvbuf, &optlen);
	fprintf(stdout, "%10d %10d %10d\n", es->sockfd, sndbuf, rcvbuf);
    }
}

static void trace_connector(struct endpoint *ep) {
    struct endsock *es, *next_es;
    int sndbuf;
    int rcvbuf;
    int optlen;
    
    fprintf(stdout, "%10s %10s %10s\n", "connector", "rcvbuf", "sndbuf");
    fprintf(stdout, "-----------------------------------------------\n");

    xendpoint_walk_sock(es, next_es, &ep->csocks) {
	sndbuf = rcvbuf = 0;
	xgetsockopt(es->sockfd, XL_SOCKET, XSNDBUF, &sndbuf, &optlen);
	xgetsockopt(es->sockfd, XL_SOCKET, XRCVBUF, &rcvbuf, &optlen);
	fprintf(stdout, "%10d %10d %10d\n", es->sockfd, sndbuf, rcvbuf);
    }
}

static void trace_bad_sockets(struct endpoint *ep) {
    struct endsock *es, *next_es;
    fprintf(stdout, "bad sockets\n");
    fprintf(stdout, "---------------------------------------------\n");

    xendpoint_walk_sock(es, next_es, &ep->bad_socks) {
	fprintf(stdout, "%10d", es->sockfd);
    }
}

void ep_strace(struct endpoint *ep) {
    trace_listener(ep);
    trace_connector(ep);
    trace_bad_sockets(ep);
}

void xep_strace(int eid) {
    ep_strace(eid_get(eid));
}
