#include <stdio.h>
#include "ep_struct.h"


int xep_rm(int efd, int sockfd) {
    struct endpoint *ep = efd_get(efd);
    struct endsock *es, *nes;

    xendpoint_walk_sock(es, nes, &ep->listener_socks) {
	if (es->sockfd != sockfd)
	    continue;
	list_del_init(&es->link);
    }
    xendpoint_walk_sock(es, nes, &ep->connector_socks) {
	if (es->sockfd != sockfd)
	    continue;
	list_del_init(&es->link);
    }
    return 0;
}
