#ifndef _HPIO_PROXY_
#define _HPIO_PROXY_

#include "core.h"


typedef int (*walkfn) (proxy_t *py, struct role *r, void *args);

static inline proxy_t *proxy_new() {
    proxy_t *py = (proxy_t *)mem_zalloc(sizeof(*py));
    return py;
}

void proxy_init(proxy_t *py);
void proxy_destroy(proxy_t *py);
int proxy_add(proxy_t *py, struct role *r);
void proxy_del(proxy_t *py, struct role *r);

struct role *proxy_lb_dispatch(proxy_t *py);
void proxy_walkr(proxy_t *py, walkfn f, void *args, int flags);
struct role *proxy_getr(proxy_t *py, uuid_t uuid);



#endif
