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

static int generic_recv(struct endpoint *ep, char **ubuf) {
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
	    list_move_tail(&at_sock->link, &ep->bad_socks);
	}
	if (list_empty(&ep->bsocks) && list_empty(&ep->csocks))
	    errno = EBADF;
    } else
	*ubuf = ephdr2ubuf(eh);
    return rc;
}

static int producer_recv(struct endpoint *ep, char **ubuf) {
    int rc;
    struct ephdr *eh;
    struct epr *rt;
    
    if ((rc = generic_recv(ep, ubuf)) == 0) {
	eh = ubuf2ephdr(*ubuf);
	rt = rt_cur(eh);
	eh->ttl--;
    }
    return rc;
}

static int comsumer_recv(struct endpoint *ep, char **ubuf) {
    int rc;
    rc = generic_recv(ep, ubuf);
    return rc;
}

typedef int (*rcvfunc) (struct endpoint *ep, char **ubuf);
const static rcvfunc recv_vfptr[] = {
    0,
    producer_recv,
    comsumer_recv,
};


int xep_recv(int efd, char **ubuf) {
    int rc;
    struct endpoint *ep = efd_get(efd);

    if (!(ep->type & (XEP_PRODUCER|XEP_COMSUMER))) {
	errno = EBADF;
	return -1;
    }
    accept_endsocks(efd);
    rc = recv_vfptr[ep->type] (ep, ubuf);
    return rc;
}

