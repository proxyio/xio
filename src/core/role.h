#ifndef _HPIO_ROLE_
#define _HPIO_ROLE_

#include "core.h"

#define IS_RCVER(r) r->type == PIO_RCVER
#define IS_SNDER(r) r->type == PIO_SNDER

int r_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened);

static inline struct role *r_new() {
    struct role *r = (struct role *)mem_zalloc(sizeof(*r));
    return r;
}

#define r_lock(r) do {				\
	spin_lock(&r->lock);			\
    } while (0)

#define r_unlock(r) do {			\
	spin_unlock(&r->lock);			\
    } while (0)


static inline int64_t __sock_read(io_t *c, char *buf, int64_t size) {
    struct role *r = container_of(c, struct role, conn_ops);
    return sk_read(r->et.fd, buf, size);
}

static inline int64_t __sock_write(io_t *c, char *buf, int64_t size) {
    struct role *r = container_of(c, struct role, conn_ops);
    return sk_write(r->et.fd, buf, size);
}

static inline void r_init(struct role *r) {
    spin_init(&r->lock);			
    r->status = ST_OK;			
    atomic_init(&r->ref);			
    atomic_set(&r->ref, 1);			
    r->et.f = r_event_handler;		
    r->et.data = r;				
    INIT_LIST_HEAD(&r->mq);			
    pp_init(&r->pp, 0);			
    r->conn_ops.read = __sock_read;		
    r->conn_ops.write = __sock_write;	
}

static inline struct role *r_new_inited() {
    struct role *r = r_new();
    if (r)
	r_init(r);
    return r;
}

static inline void r_destroy(struct role *r) {
    spin_destroy(&r->lock);			
    pp_destroy(&r->pp);			
    atomic_destroy(&r->ref);		
}

static inline void r_free_destroy(struct role *r) {
    r_destroy(r);
    mem_free(r, sizeof(*r));
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

static inline pio_msg_t *r_pop(struct role *r) {
    pio_msg_t *msg = NULL;
    r_lock(r);
    if (!list_empty(&r->mq))
	msg = list_first(&r->mq, pio_msg_t, node);
    if (list_empty(&r->mq))
	__r_disable_eventout(r);
    r_unlock(r);
    return msg;
}

static inline int r_push(struct role *r, pio_msg_t *msg) {
    r_lock(r);
    r->size++;
    list_add(&msg->node, &r->mq);
    if (!list_empty(&r->mq))
	__r_enable_eventout(r);
    r_unlock(r);
    return 0;
}

#endif
