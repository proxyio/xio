#include <stdio.h>
#include <os/alloc.h>
#include "pipe_struct.h"


int xpipeline_add(int xpipefd, int xsockfd) {
    struct pipe_struct *ps = pfd_get(xpipefd);
    struct endpoint *ep = (struct endpoint *)mem_zalloc(sizeof(*ep));

    if (!ep)
	return -1;
    ep->sockfd = xsockfd;
    list_add_tail(&ep->link, &ps->endpoints);
    return 0;
}
