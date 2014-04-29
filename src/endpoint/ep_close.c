#include <stdio.h>
#include "ep_struct.h"

static void endpoint_exit(struct endpoint *ep) {
    INIT_LIST_HEAD(&ep->bsocks);
    INIT_LIST_HEAD(&ep->csocks);
}

void xep_close(int efd) {
    struct endpoint *ep = efd_get(efd);
    endpoint_exit(ep);
    efd_free(efd);
}
