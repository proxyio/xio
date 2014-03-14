#ifndef _HPIO_RIO_
#define _HPIO_RIO_

#include "core.h"

#define IS_RCVER(r) r->io.rgh.type == PIO_RCVER
#define IS_SNDER(r) r->io.rgh.type == PIO_SNDER

int r_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened);

static inline struct rio *r_new() {
    struct rio *r = (struct rio *)mem_zalloc(sizeof(*r));
    return r;
}

#define r_lock(r) do {				\
	spin_lock(&r->lock);			\
    } while (0)

#define r_unlock(r) do {			\
	spin_unlock(&r->lock);			\
    } while (0)

static inline void r_init(struct rio *r) {
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

static inline struct rio *r_new_inited() {
    struct rio *r = r_new();
    if (r)
	r_init(r);
    return r;
}

static inline void r_destroy(struct rio *r) {
    proxyio_destroy(&r->io);
    spin_destroy(&r->lock);			
    atomic_destroy(&r->ref);		
    mem_cache_destroy(&r->slabs);
}

static inline void r_put(struct rio *r) {
    if (atomic_dec(&r->ref, 1) == 1) {
	r_destroy(r);
	mem_free(r, sizeof(*r));
    }
}


static inline int __r_disable_eventout(struct rio *r) {
    if (r->et.events & EPOLLOUT) {
	r->et.events &= ~EPOLLOUT;
	return epoll_mod(r->el, &r->et);
    }
    return -1;
}

static inline int __r_enable_eventout(struct rio *r) {
    if (!(r->et.events & EPOLLOUT)) {
	r->et.events |= EPOLLOUT;
	return epoll_mod(r->el, &r->et);
    }
    return -1;
}

static inline pio_msg_t *r_pop_massage(struct rio *r) {
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

static inline int r_push_massage(struct rio *r, pio_msg_t *msg) {
    r_lock(r);
    r->mqsize++;
    list_add(&msg->node, &r->mq);
    if (!list_empty(&r->mq))
	__r_enable_eventout(r);
    r_unlock(r);
    return 0;
}

#endif
