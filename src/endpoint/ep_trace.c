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

void eid_strace(int eid) {
    ep_strace(eid_get(eid));
}
