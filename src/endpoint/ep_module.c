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

int eid_alloc() {
    int eid;
    mutex_lock(&epgb.lock);
    BUG_ON(epgb.nendpoints >= XIO_MAX_ENDPOINTS);
    eid = epgb.unused[epgb.nendpoints++];
    mutex_unlock(&epgb.lock);
    return eid;
}

void eid_free(int eid) {
    mutex_lock(&epgb.lock);
    epgb.unused[--epgb.nendpoints] = eid;
    mutex_unlock(&epgb.lock);
}

struct endpoint *eid_get(int eid) {
    return &epgb.endpoints[eid];
}

void accept_endsocks(int eid) {
    struct endpoint *ep = eid_get(eid);
    int tmp, s;
    struct endsock *es, *next_es;

    xendpoint_walk_sock(es, next_es, &ep->bsocks) {
	if (xselect(XPOLLIN|XPOLLERR, 1, &es->sockfd, 1, &tmp) == 0)
	    continue;
	BUG_ON(es->sockfd != tmp);
	if ((s = xaccept(es->sockfd)) < 0) {
	    if (errno != EAGAIN)
		list_move_tail(&es->link, &ep->bad_socks);
	    DEBUG_OFF("listener %d bad status", es->sockfd);
	    continue;
	}
	if (xep_add(eid, s) < 0) {
	    xclose(s);
	    continue;
	}
	DEBUG_OFF("accept new endsock %d", s);	
    }
}

