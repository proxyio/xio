#include <stdio.h>
#include "net/accepter.h"
#include "role.h"
#include "grp.h"

int acp_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened) {
    int nfd;
    struct role *r = r_new_inited();

    if (!r)
	return -1;
    if (!(happened & EPOLLIN) || (nfd = act_accept(et->fd)) < 0) {
	mem_free(r, sizeof(*r));
	return -1;
    }
    r->type = ST_REGISTER;
    r->el = el;
    r->et.fd = nfd;
    r->et.events = EPOLLIN;
    if (epoll_add(el, &r->et) < 0) {
	close(nfd);
	r_free_destroy(r);
    }
    return 0;
}
