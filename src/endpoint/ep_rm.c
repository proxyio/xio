#include <stdio.h>
#include "ep_struct.h"


int xep_rm(int eid, int sockfd) {
    struct endpoint *ep = eid_get(eid);
    struct endsock *es, *nes;

    xendpoint_walk_sock(es, nes, &ep->bsocks) {
	if (es->sockfd != sockfd)
	    continue;
	list_del_init(&es->link);
    }
    xendpoint_walk_sock(es, nes, &ep->csocks) {
	if (es->sockfd != sockfd)
	    continue;
	list_del_init(&es->link);
    }
    return 0;
}
