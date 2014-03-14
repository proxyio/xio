#include <stdio.h>
#include "net/accepter.h"
#include "rio.h"
#include "proxy.h"

int acp_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened) {
    int nfd;
    struct rio *r;

    if (!(happened & EPOLLIN) || (nfd = act_accept(et->fd)) < 0)
	return -1;
    sk_setopt(nfd, SK_NONBLOCK, true);
    if ((r = r_new_inited())) {
	r->is_register = true;
	r->el = el;
	r->et.fd = r->io.sockfd = nfd;
	r->et.events = EPOLLIN;
	if (epoll_add(el, &r->et) < 0) {
	    close(nfd);
	    r_put(r);
	} else
	    return 0;
    }
    return -1;
}
