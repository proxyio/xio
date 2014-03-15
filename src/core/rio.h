#ifndef _HPIO_RIO_
#define _HPIO_RIO_

#include "core.h"

#define IS_RCVER(r) r->io.rgh.type == PIO_RCVER
#define IS_SNDER(r) r->io.rgh.type == PIO_SNDER

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

void r_init(struct rio *r);
void r_destroy(struct rio *r);


static inline struct rio *r_new_inited() {
    struct rio *r = r_new();
    if (r)
	r_init(r);
    return r;
}


#endif
