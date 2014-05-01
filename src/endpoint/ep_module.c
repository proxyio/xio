#include <stdio.h>
#include <xio/socket.h>
#include <xio/poll.h>
#include "ep_struct.h"

struct xep_global epgb = {};

void xep_module_init() {
    int i;
    epgb.exiting = true;
    mutex_init(&epgb.lock);

    for (i = 0; i < XIO_MAX_ENDPOINTS; i++) {
	epgb.unused[i] = i;
    }
    epgb.nendpoints = 0;
}

void xep_module_exit() {
    epgb.exiting = true;
    mutex_destroy(&epgb.lock);
}

int efd_alloc() {
    int efd;
    mutex_lock(&epgb.lock);
    BUG_ON(epgb.nendpoints >= XIO_MAX_ENDPOINTS);
    efd = epgb.unused[epgb.nendpoints++];
    mutex_unlock(&epgb.lock);
    return efd;
}

void efd_free(int efd) {
    mutex_lock(&epgb.lock);
    epgb.unused[--epgb.nendpoints] = efd;
    mutex_unlock(&epgb.lock);
}

struct endpoint *efd_get(int efd) {
    return &epgb.endpoints[efd];
}

void accept_endsocks(int efd) {
    struct endpoint *ep = efd_get(efd);
    int tmp, s;
    struct endsock *es, *next_es;

    xendpoint_walk_sock(es, next_es, &ep->bsocks) {
	if (xselect(XPOLLIN|XPOLLERR, 1, &es->sockfd, 1, &tmp) == 0)
	    continue;
	BUG_ON(es->sockfd != tmp);
	DEBUG_ON("endsocks accept start");
	if ((s = xaccept(es->sockfd)) < 0) {
	    if (errno != EAGAIN)
		list_move_tail(&es->link, &ep->bad_socks);
	    continue;
	}
	DEBUG_ON("endsocks accept end");
	if (xep_add(efd, s) < 0)
	    xclose(s);
    }
}

