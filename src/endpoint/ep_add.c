#include <stdio.h>
#include <os/alloc.h>
#include <xio/socket.h>
#include "ep_struct.h"


int xep_add(int eid, int sockfd) {
    struct endpoint *ep = eid_get(eid);
    int fnb = 1;
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
    xsetsockopt(sockfd, XL_SOCKET, XNOBLOCK, &fnb, sizeof(fnb));
    xgetsockopt(sockfd, XL_SOCKET, XSOCKTYPE, &socktype, &optlen);
    head = socktype == XCONNECTOR ? &ep->csocks : &ep->bsocks;
    list_add_tail(&s->link, head);
    if (ep->type == XEP_PRODUCER)
	uuid_generate(s->uuid);
    DEBUG_OFF("endpoint %d add %d socket", eid, sockfd);
    return 0;
}
