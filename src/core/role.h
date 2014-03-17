#ifndef _HPIO_ROLE_
#define _HPIO_ROLE_

#include "core.h"

#define IS_RCVER(r) r->pp.rgh.type == PIO_RCVER
#define IS_SNDER(r) r->pp.rgh.type == PIO_SNDER

static inline struct role *r_new() {
    struct role *r = (struct role *)mem_zalloc(sizeof(*r));
    return r;
}

void r_init(struct role *r);
void r_destroy(struct role *r);


static inline struct role *r_new_inited() {
    struct role *r = r_new();
    if (r)
	r_init(r);
    return r;
}


#endif
