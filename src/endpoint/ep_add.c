#include <stdio.h>
#include <os/alloc.h>
#include <xio/socket.h>
#include "ep_struct.h"


int xep_add(int efd, int sockfd) {
    int socktype = XCONNECTOR;
    int optlen = sizeof(socktype);
    struct endpoint *ep = efd_get(efd);
    struct endsock *s = (struct endsock *)mem_zalloc(sizeof(*s));

    if (!s)
	return -1;
    s->sockfd = sockfd;
    xgetsockopt(sockfd, XL_SOCKET, XSOCKTYPE, &socktype, &optlen);
    switch (socktype) {
    case XCONNECTOR:
	list_add_tail(&s->link, &ep->connector_socks);
	break;
    case XLISTENER:
	list_add_tail(&s->link, &ep->listener_socks);
	break;
    default:
	free(s);
	errno = EBADF;
	return -1;
    }
    return 0;
}
