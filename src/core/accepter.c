#include "net/accepter.h"
#include "role.h"
#include "grp.h"

int accepter_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened) {
    int nfd;
    struct role *r = r_new();

    if (!r)
	return -1;
    r_init(r);
    if (!(happened & EPOLLIN) || (nfd = act_accept(et->fd)) < 0) {
	mem_free(r, sizeof(*r));
	return -1;
    }
    r->status = ST_REGISTER;
    r->el = el;
    r->et.fd = nfd;
    r->et.to_nsec = 0;
    r->et.events = EPOLLIN;
    return epoll_add(el, &r->et);
}
