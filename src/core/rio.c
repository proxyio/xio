#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "rio.h"
#include "proxy.h"
#include "accepter.h"

static void r_rgs(struct rio *r, uint32_t happened);
static void r_recv(struct rio *r);
static void r_send(struct rio *r);
static void r_error(struct rio *r);

int r_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened) {
    struct rio *r = container_of(et, struct rio, et);
    
    if (!r->registed)
	r_rgs(r, happened);
    if (r->registed) {
	if (r->status_ok && (happened & EPOLLIN)) {

	    r_recv(r);
	}
	if (r->status_ok && (happened & EPOLLOUT))
	    r_send(r);
    }
    if (!r->status_ok)
	r_error(r);
    return 0;
}

static void r_rgs(struct rio *r, uint32_t happened) {
    int ret;
    struct pio_rgh *h = &r->io.rgh;
    proxy_t *py;
    acp_t *acp = container_of(r->el, struct accepter, el);

    if ((ret = r->is_register ?
	 proxyio_ps_rgs(&r->io) : proxyio_at_rgs(&r->io)) < 0) {
	r->status_ok = (errno != EAGAIN) ? false : true;
	return;
    }
    if (!(py = acp_find(acp, h->proxyname))) {
	r->status_ok = false;
	return;
    }
    r->registed = true;
    r->py = py;
    proxy_add(py, r);
}

static int __r_receiver_recv(struct rio *r) {
    pio_msg_t *msg = mem_cache_alloc(&r->slabs);
    int64_t now = rt_mstime();
    struct rio *dest;
    struct pio_rt *crt;

    if (!msg)
	return -1;
    if (proxyio_recv(&r->io, &msg->hdr, &msg->data, (char **)&msg->rt) < 0
	|| !ph_validate(&msg->hdr)) {
	mem_cache_free(&r->slabs, msg);
	r->status_ok = (errno != EAGAIN) ? false : true;
	return -1;
    }
    crt = pio_msg_currt(msg);
    crt->cost = (uint16_t)(now - msg->hdr.sendstamp - crt->begin);
    if (!(dest = proxy_lb_dispatch(r->py)) || r_push_massage(dest, msg) < 0) {
	pio_msg_free_data_and_rt(msg);
	mem_cache_free(&r->slabs, msg);
	return -1;
    }
    return 0;
}

static void r_receiver_recv(struct rio *r) {
    if (proxyio_prefetch(&r->io) < 0 && errno != EAGAIN) {
	r->status_ok = false;
	return;
    }
    while (__r_receiver_recv(r) == 0) {
    }
}


static int __r_dispatcher_recv(struct rio *r) {
    pio_msg_t *msg = mem_cache_alloc(&r->slabs);
    struct rio *src;
    struct pio_rt *crt;

    if (!msg)
	return -1;
    if (proxyio_recv(&r->io, &msg->hdr, &msg->data, (char **)&msg->rt) < 0
	|| !ph_validate(&msg->hdr)) {
	mem_cache_free(&r->slabs, msg);
	r->status_ok = (errno != EAGAIN) ? false : true;
	return -1;
    }
    msg->hdr.ttl--;
    ph_makechksum(&msg->hdr);
    crt = pio_msg_currt(msg);
    if (!(src = proxy_find_at(r->py, crt->uuid)) || r_push_massage(src, msg) < 0) {
	pio_msg_free_data_and_rt(msg);
	mem_cache_free(&r->slabs, msg);
	return -1;
    }
    return 0;
}

static void r_dispatcher_recv(struct rio *r) {
    if (proxyio_prefetch(&r->io) < 0 && errno != EAGAIN) {
	r->status_ok = false;
	return;
    }
    while (__r_dispatcher_recv(r) == 0) {
    }
}


static void r_recv(struct rio *r) {
    if (IS_RCVER(r))
	return r_receiver_recv(r);
    return r_dispatcher_recv(r);
}

static void r_receiver_send(struct rio *r) {
    pio_msg_t *msg;

    if (!(msg = r_pop_massage(r)))
	return;
    proxyio_send(&r->io, &msg->hdr, msg->data, (char *)msg->rt);
    while (proxyio_flush(&r->io) < 0)
	if (errno != EAGAIN) {
	    r->status_ok = false;
	    break;
	}
    pio_msg_free_data_and_rt(msg);
    mem_cache_free(&r->slabs, msg);
}

static void r_dispatcher_send(struct rio *r) {
    pio_msg_t *msg;
    int64_t now = rt_mstime();
    struct pio_rt rt = {}, *crt;
    
    if (!(msg = r_pop_massage(r)))
	return;
    crt = pio_msg_currt(msg);
    uuid_copy(rt.uuid, r->io.rgh.id);
    rt.begin = (uint32_t)(now - msg->hdr.sendstamp);
    crt->stay = (uint16_t)(rt.begin - crt->begin - crt->cost);
    if (!pio_rt_append(msg, &rt))
	goto EXIT;
    proxyio_send(&r->io, &msg->hdr, msg->data, (char *)msg->rt);
    while (proxyio_flush(&r->io) < 0) {
	if (errno != EAGAIN) {
	    r->status_ok = false;
	    break;
	}
    }
 EXIT:
    pio_msg_free_data_and_rt(msg);
    mem_cache_free(&r->slabs, msg);
}

static void r_send(struct rio *r) {
    if (IS_RCVER(r))
	return r_receiver_send(r);
    return r_dispatcher_send(r);
}


static void r_error(struct rio *r) {
    epoll_del(r->el, &r->et);
    close(r->et.fd);
    proxy_del(r->py, r);
    r_put(r);
}
