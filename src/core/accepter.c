#include <stdio.h>
#include "net/accepter.h"
#include "role.h"
#include "grp.h"

int acp_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened) {
    int nfd;
    struct role *r;

    if (!(happened & EPOLLIN) || (nfd = act_accept(et->fd)) < 0)
	return -1;
    if ((r = r_new_inited())) {
	r->type = ST_REGISTER;
	r->el = el;
	r->et.fd = nfd;
	r->et.events = EPOLLIN;
	if (epoll_add(el, &r->et) < 0) {
	    close(nfd);
	    r_put(r);
	} else
	    return 0;
    }
    return -1;
}
