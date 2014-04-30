#include <stdio.h>
#include <os/alloc.h>
#include <xio/socket.h>
#include "ep_struct.h"


int xep_add(int efd, int sockfd) {
    struct endpoint *ep = efd_get(efd);
    int socktype = XCONNECTOR;
    int optlen = sizeof(socktype);
    struct endsock *s;
    struct list_head *head;

    if (!(socktype & (XCONNECTOR|XLISTENER))) {
	errno = EBADF;
	return -1;
    }
    if (!(s = (struct endsock *)mem_zalloc(sizeof(*s))))
	return -1;
    s->sockfd = sockfd;
    xgetsockopt(sockfd, XL_SOCKET, XSOCKTYPE, &socktype, &optlen);
    head = socktype == XCONNECTOR ? &ep->csocks : &ep->bsocks;
    list_add_tail(&s->link, head);
    return 0;
}
