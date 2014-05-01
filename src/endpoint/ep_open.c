#include <stdio.h>
#include "ep_struct.h"

int xep_open(int type) {
    int efd;
    struct endpoint *ep;

    if (!(type & (XEP_PRODUCER|XEP_COMSUMER))) {
	errno = EINVAL;
	return -1;
    }
    if ((efd = efd_alloc()) < 0) {
	errno = EMFILE;
	return -1;
    }
    ep = efd_get(efd);
    ep->type = type;
    INIT_LIST_HEAD(&ep->bsocks);
    INIT_LIST_HEAD(&ep->csocks);
    INIT_LIST_HEAD(&ep->bad_socks);
    return efd;
}
