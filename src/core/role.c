#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "role.h"
#include "proxy.h"
#include "accepter.h"

static int r_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened);
static void r_rgs(struct role *r, uint32_t happened);
static void r_recv(struct role *r);
static void r_send(struct role *r);
static void r_error(struct role *r);

void r_init(struct role *r) {
    proxyio_init(&r->io);
    spin_init(&r->lock);
    r->registed = false;
    r->status_ok = true;
    r->is_register = false;
    atomic_init(&r->ref);
    atomic_set(&r->ref, 1);
    r->et.f = r_event_handler;
    r->et.data = r;
    r->mqsize = 0;
    INIT_LIST_HEAD(&r->mq);
    r->sibling.key = (char *)r->io.rgh.id;
    r->sibling.keylen = sizeof(r->io.rgh.id);
    mem_cache_init(&r->slabs, sizeof(pio_msg_t));
}

void r_destroy(struct role *r) {
    proxyio_destroy(&r->io);
    spin_destroy(&r->lock);			
    atomic_destroy(&r->ref);		
    mem_cache_destroy(&r->slabs);
}

void r_put(struct role *r) {
    if (atomic_dec(&r->ref, 1) == 1) {
	r_destroy(r);
	mem_free(r, sizeof(*r));
    }
}


static inline int __r_disable_eventout(struct role *r) {
    if (r->et.events & EPOLLOUT) {
	r->et.events &= ~EPOLLOUT;
	return epoll_mod(r->el, &r->et);
    }
    return -1;
}

static inline int __r_enable_eventout(struct role *r) {
    if (!(r->et.events & EPOLLOUT)) {
	r->et.events |= EPOLLOUT;
	return epoll_mod(r->el, &r->et);
    }
    return -1;
}

static pio_msg_t *r_pop_massage(struct role *r) {
    pio_msg_t *msg = NULL;
    r_lock(r);
    if (!list_empty(&r->mq)) {
	r->mqsize--;
	msg = list_first(&r->mq, pio_msg_t, node);
	list_del(&msg->node);
    }
    if (list_empty(&r->mq))
	__r_disable_eventout(r);
    r_unlock(r);
    return msg;
}

static int r_push_massage(struct role *r, pio_msg_t *msg) {
    r_lock(r);
    r->mqsize++;
    list_add_tail(&msg->node, &r->mq);
    if (!list_empty(&r->mq))
	__r_enable_eventout(r);
    r_unlock(r);
    return 0;
}

static int r_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened) {
    struct role *r = container_of(et, struct role, et);
    if (!r->registed)
	r_rgs(r, happened);
    if (r->registed) {
	if (r->status_ok && (happened & EPOLLIN))
	    r_recv(r);
	if (r->status_ok && (happened & EPOLLOUT))
	    r_send(r);
    }
    if (!r->status_ok)
	r_error(r);
    return 0;
}

static void r_rgs(struct role *r, uint32_t happened) {
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

static int __r_receiver_recv(struct role *r) {
    pio_msg_t *msg = mem_cache_alloc(&r->slabs);
    int64_t now = rt_mstime();
    struct role *dest;
    struct pio_rt *crt;

    if (!msg)
	return -1;
    if (proxyio_bread(&r->io, &msg->hdr, &msg->data, (char **)&msg->rt) < 0
	|| !ph_validate(&msg->hdr)) {
	mem_cache_free(&r->slabs, msg);
	r->status_ok = (errno != EAGAIN) ? false : true;
	return -1;
    }
    crt = pio_msg_currt(msg);
    crt->cost = (uint16_t)(now - msg->hdr.sendstamp - crt->go);
    if (!(dest = proxy_lb_dispatch(r->py)) || r_push_massage(dest, msg) < 0) {
	pio_msg_free_data_and_rt(msg);
	mem_cache_free(&r->slabs, msg);
	return -1;
    }
    return 0;
}

static void r_receiver_recv(struct role *r) {
    if (proxyio_prefetch(&r->io) < 0 && errno != EAGAIN) {
	r->status_ok = false;
	return;
    }
    while (__r_receiver_recv(r) == 0) {
    }
}


static int __r_dispatcher_recv(struct role *r) {
    pio_msg_t *msg = mem_cache_alloc(&r->slabs);
    struct role *src;
    struct pio_rt *crt;

    if (!msg)
	return -1;
    if (proxyio_bread(&r->io, &msg->hdr, &msg->data, (char **)&msg->rt) < 0
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

static void r_dispatcher_recv(struct role *r) {
    if (proxyio_prefetch(&r->io) < 0 && errno != EAGAIN) {
	r->status_ok = false;
	return;
    }
    while (__r_dispatcher_recv(r) == 0) {
    }
}


static void r_recv(struct role *r) {
    if (IS_RCVER(r))
	return r_receiver_recv(r);
    return r_dispatcher_recv(r);
}

static void r_receiver_send(struct role *r) {
    pio_msg_t *msg;

    if (!(msg = r_pop_massage(r)))
	return;
    proxyio_bwrite(&r->io, &msg->hdr, msg->data, (char *)msg->rt);
    while (proxyio_flush(&r->io) < 0)
	if (errno != EAGAIN) {
	    r->status_ok = false;
	    break;
	}
    pio_msg_free_data_and_rt(msg);
    mem_cache_free(&r->slabs, msg);
}

static void r_dispatcher_send(struct role *r) {
    pio_msg_t *msg;
    int64_t now = rt_mstime();
    struct pio_rt rt = {}, *crt;
    
    if (!(msg = r_pop_massage(r)))
	return;
    crt = pio_msg_currt(msg);
    uuid_copy(rt.uuid, r->io.rgh.id);
    rt.go = (uint32_t)(now - msg->hdr.sendstamp);
    crt->stay = (uint16_t)(rt.go - crt->go - crt->cost);
    if (!pio_rt_append(msg, &rt))
	goto EXIT;
    proxyio_bwrite(&r->io, &msg->hdr, msg->data, (char *)msg->rt);
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

static void r_send(struct role *r) {
    if (IS_RCVER(r))
	return r_receiver_send(r);
    return r_dispatcher_send(r);
}


static void r_error(struct role *r) {
    epoll_del(r->el, &r->et);
    close(r->et.fd);
    proxy_del(r->py, r);
    r_put(r);
}
