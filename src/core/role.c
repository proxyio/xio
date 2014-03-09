#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "role.h"
#include "grp.h"
#include "accepter.h"

static int role_rgs(struct role *r, uint32_t happened);
static int role_recv(struct role *r);
static int role_send(struct role *r);
static int role_error(struct role *r);

int role_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened) {
    struct role *r = container_of(et, struct role, et);

    if (!(r->status & ST_REGISTED))
	return role_rgs(r, happened);
    if (!(r->status & ST_REGISTED))
	return -1;
    if ((r->status & ST_OK) && (happened & EPOLLIN))
	role_recv(r);
    if ((r->status & ST_OK) && (happened & EPOLLOUT))
	role_send(r);
    if (!(r->status & ST_OK))
	role_error(r);
    return 0;
}

static inline int r_connector_rgs(struct role *r, uint32_t happened) {
    int ret;
    epoll_t *el = r->el;
    epollevent_t *et = &r->et;
    struct pio_rgh *h = &r->pp.rgh;
    grp_t *grp;
    accepter_t *acp = container_of(el, struct accepter, el);

    if (!(happened & EPOLLOUT))
	return -1;
    ret = pp_send_rgh_async(&r->pp, &r->conn_ops);
    if (ret < 0 && errno == EAGAIN)
	return -1;
    if (ret == 0 && !(grp = accepter_find(acp, h->grpname))) {
	r_destroy(r);
	epoll_del(el, et);
	close(et->fd);
	mem_free(r, sizeof(*r));
	return -1;
    } else if (ret < 0 && errno != EAGAIN) {
	epoll_del(el, et);
	if (sk_reconnect(&et->fd) == 0) {
	    pp_reset_rgh(&r->pp);
	    epoll_add(el, et);
	} else {
	    close(et->fd);
	    mem_free(r, sizeof(*r));
	}
	return -1;
    } else {
	r->status = ST_REGISTED | ST_OK;
	r->type = h->type;
	uuid_copy(r->uuid, h->id);
	r->grp = grp;
	grp_add(grp, r);
    }
    return 0;
}

static inline int r_register_rgs(struct role *r, uint32_t happened) {
    int ret;
    epoll_t *el = r->el;
    epollevent_t *et = &r->et;
    struct pio_rgh *h = &r->pp.rgh;
    grp_t *grp;
    accepter_t *acp = container_of(el, struct accepter, el);

    if (!(happened & EPOLLIN))
	return -1;
    if ((ret = pp_recv_rgh(&r->pp, &r->conn_ops)) < 0 && errno == EAGAIN) {
	return -1;
    } else if ((ret == 0 && !(grp = accepter_find(acp, h->grpname))) ||
	       (ret < 0 && errno != EAGAIN)) {
	r_destroy(r);
	epoll_del(el, et);
	close(et->fd);
	mem_free(r, sizeof(*r));
    } else {
	r->status = ST_REGISTED | ST_OK;
	r->type = h->type;
	uuid_copy(r->uuid, h->id);
	r->grp = grp;
	grp_add(grp, r);
    }
    return 0;
}

static int role_rgs(struct role *r, uint32_t happened) {
    if ((r->status & ST_REGISTER))
	return r_register_rgs(r, happened);
    return r_connector_rgs(r, happened);
}

static int role_recv(struct role *r) {
    return 0;
}

static int role_send(struct role *r) {
    epoll_t *el = r->el;
    epollevent_t *et = &r->et;

    et->events &= ~EPOLLOUT;
    epoll_mod(el, et);
    return 0;
}

static int role_error(struct role *r) {
    return 0;
}
