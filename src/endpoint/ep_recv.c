#include <stdio.h>
#include <xio/socket.h>
#include <xio/poll.h>
#include "ep_struct.h"

static struct endsock *get_available_csock(int efd) {
    struct endsock *es, *next_es;
    struct endpoint *ep = efd_get(efd);
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

int xep_recv(int efd, void **xbuf) {
    char *xmsg;
    struct endsock *at_sock;
    struct endpoint *ep = efd_get(efd);

    accept_incoming_endsocks(efd);
    if ((at_sock = get_available_csock(efd)) == 0) {
	errno = EAGAIN;
	return -1;
    }
    if (xrecv(at_sock->sockfd, &xmsg) < 0) {
	if (errno != EAGAIN)
	    list_move_tail(&at_sock->link, &ep->bad_socks);
	errno = EAGAIN;
	if (list_empty(&ep->bsocks) && list_empty(&ep->csocks))
	    errno = EBADF;
	return -1;
    }
    return 0;
}

