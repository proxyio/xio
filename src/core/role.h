#ifndef _HPIO_ROLE_
#define _HPIO_ROLE_

#include "core.h"

#define IS_RCVER(r) r->type == PIO_RCVER
#define IS_SNDER(r) r->type == PIO_SNDER

int role_event_handler(epoll_t *el, epollevent_t *et, uint32_t happened);

static inline struct role *r_new() {
    struct role *r = (struct role *)mem_zalloc(sizeof(*r));
    return r;
}

static inline int64_t __sock_read(io_t *c, char *buf, int64_t size) {
    struct role *r = container_of(c, struct role, conn_ops);
    return sk_read(r->et.fd, buf, size);
}

static inline int64_t __sock_write(io_t *c, char *buf, int64_t size) {
    struct role *r = container_of(c, struct role, conn_ops);
    return sk_write(r->et.fd, buf, size);
}

#define r_init(r) do {				\
	epollevent_t *__et = &r->et;		\
	spin_init(&r->lock);			\
	__et->f = role_event_handler;		\
	__et->data = r;				\
	INIT_LIST_HEAD(&r->mq_link);		\
	INIT_LIST_HEAD(&r->grp_link);		\
	pp_init(&r->pp, 0);			\
	r->conn_ops.read = __sock_read;		\
	r->conn_ops.write = __sock_write;	\
    } while (0)

#define r_destroy(r) do {			\
	spin_destroy(&r->lock);			\
    } while (0)

#define r_lock(r) do {				\
	spin_lock(&r->lock);			\
    } while (0)

#define r_unlock(r) do {			\
	spin_unlock(&r->lock);			\
    } while (0)




#endif
