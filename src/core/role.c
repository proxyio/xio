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
    proto_parser_init(&r->pp);
    spin_init(&r->lock);
    r->registed = false;
    r->status_ok = true;
    r->proxyto = false;
    atomic_init(&r->ref);
    atomic_set(&r->ref, 1);
    r->et.f = r_event_handler;
    r->et.data = r;
    r->mqsize = 0;
    INIT_LIST_HEAD(&r->mq);
    r->sibling.key = (char *)r->pp.rgh.id;
    r->sibling.keylen = sizeof(r->pp.rgh.id);
    mem_cache_init(&r->slabs, sizeof(pio_msg_t));
}

void r_destroy(struct role *r) {
    pio_msg_t *msg, *tmp;

    list_for_each_msg_safe(msg, tmp, &r->mq) {
	list_del(&msg->mq_link);
	pio_msg_free_data_and_rt(msg);
	mem_free(msg, sizeof(*msg));
    }
    proto_parser_destroy(&r->pp);
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
	msg = list_first(&r->mq, pio_msg_t, mq_link);
	list_del(&msg->mq_link);
    }
    if (list_empty(&r->mq))
	__r_disable_eventout(r);
    r_unlock(r);
    return msg;
}

static int r_push_massage(struct role *r, pio_msg_t *msg) {
    r_lock(r);
    r->mqsize++;
    list_add_tail(&msg->mq_link, &r->mq);
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
    struct pio_rgh *h = &r->pp.rgh;
    proxy_t *py;
    acp_t *acp = container_of(r->el, struct accepter, el);

    ret = r->proxyto ?  proto_parser_at_rgs(&r->pp) : proto_parser_ps_rgs(&r->pp);
    if (ret < 0) {
	r->status_ok = (errno != EAGAIN) ? false : true;
	return;
    }
    if (!(py = acp_find(acp, h->proxyname)) || proxy_add(py, r) != 0)
	r->status_ok = false;
    else {
	r->py = py;
	r->registed = true;
    }
}

static int __r_receiver_recv(struct role *r) {
    pio_msg_t *msg = mem_cache_alloc(&r->slabs);
    int64_t now = rt_mstime();
    struct role *dest;

    if (!msg)
	return -1;
    if (proto_parser_bread(&r->pp, &msg->hdr, &msg->data, (char **)&msg->rt) < 0
	|| !ph_validate(&msg->hdr)) {
	mem_cache_free(&r->slabs, msg);
	r->status_ok = (errno != EAGAIN) ? false : true;
	return -1;
    }
    rt_go_cost(msg, now);
    if (!(dest = proxy_lb_dispatch(r->py)) || r_push_massage(dest, msg) < 0) {
	pio_msg_free_data_and_rt(msg);
	mem_cache_free(&r->slabs, msg);
	return -1;
    }
    return 0;
}

static void r_receiver_recv(struct role *r) {
    if (proto_parser_prefetch(&r->pp) < 0 && errno != EAGAIN) {
	r->status_ok = false;
	return;
    }
    while (__r_receiver_recv(r) == 0) {
    }
}


static int __r_dispatcher_recv(struct role *r) {
    int64_t now = rt_mstime();
    pio_msg_t *msg = mem_cache_alloc(&r->slabs);
    struct role *src;
    struct pio_rt *rt;

    if (!msg)
	return -1;
    if (proto_parser_bread(&r->pp, &msg->hdr, &msg->data, (char **)&msg->rt) < 0
	|| !ph_validate(&msg->hdr)) {
	mem_cache_free(&r->slabs, msg);
	r->status_ok = (errno != EAGAIN) ? false : true;
	return -1;
    }
    rt_back_cost(msg, now);
    rt = pio_msg_prevrt(msg);
    if (!(src = proxy_find_at(r->py, rt->uuid)) || r_push_massage(src, msg) < 0) {
	pio_msg_free_data_and_rt(msg);
	mem_cache_free(&r->slabs, msg);
	return -1;
    }
    return 0;
}

static void r_dispatcher_recv(struct role *r) {
    if (proto_parser_prefetch(&r->pp) < 0 && errno != EAGAIN) {
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
    int64_t now = rt_mstime();
    pio_msg_t *msg;

    if (!(msg = r_pop_massage(r)))
	return;
    rt_shrink_and_back(msg, now);
    proto_parser_bwrite(&r->pp, &msg->hdr, msg->data, (char *)msg->rt);
    while (proto_parser_flush(&r->pp) < 0)
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
    struct pio_rt rt = {};
    
    if (!(msg = r_pop_massage(r)))
	return;
    uuid_copy(rt.uuid, r->pp.rgh.id);
    if (!rt_append_and_go(msg, &rt, now))
	goto EXIT;
    proto_parser_bwrite(&r->pp, &msg->hdr, msg->data, (char *)msg->rt);
    while (proto_parser_flush(&r->pp) < 0) {
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
    if (r->py)
	proxy_del(r->py, r);
    r_put(r);
}
