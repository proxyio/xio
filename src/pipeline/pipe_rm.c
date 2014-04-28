#include <stdio.h>
#include "pipe_struct.h"


int xpipeline_rm(int xpipefd, int xsockfd) {
    struct pipe_struct *ps = pfd_get(xpipefd);
    struct endpoint *ep, *nep;

    xpipe_walk_endpoint(ep, nep, &ps->endpoints) {
	if (ep->sockfd != xsockfd)
	    continue;
	list_del_init(&ep->link);
    }
    return 0;
}
