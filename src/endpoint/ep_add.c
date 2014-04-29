#include <stdio.h>
#include <os/alloc.h>
#include "ep_struct.h"


int xep_add(int efd, int sockfd) {
    struct endpoint *ep = efd_get(efd);
    struct endsock *s = (struct endsock *)mem_zalloc(sizeof(*s));

    if (!s)
	return -1;
    s->sockfd = sockfd;
    list_add_tail(&s->link, &ep->listener_socks);
    return 0;
}
