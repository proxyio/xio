#include <stdio.h>
#include "ep_struct.h"

static void endpoint_init(struct endpoint *ep) {
    INIT_LIST_HEAD(&ep->listener_socks);
    INIT_LIST_HEAD(&ep->connector_socks);
}

int xep_open() {
    int efd = efd_alloc();
    struct endpoint *ep;
    
    if (efd < 0) {
	errno = EMFILE;
	return -1;
    }
    ep = efd_get(efd);
    endpoint_init(ep);
    return efd;
}
