#ifndef _HPIO_PROXY_
#define _HPIO_PROXY_

#include "core.h"


typedef int (*walkfn) (proxy_t *py, struct role *r, void *args);

#define list_for_each_role_safe(pos, n, head)				\
    list_for_each_entry_safe(pos, n, head, struct role, py_link)

#define proxy_lock(py) spin_lock(&py->lock)
#define proxy_unlock(py) spin_unlock(&py->lock)

static inline proxy_t *proxy_new() {
    proxy_t *py = (proxy_t *)mem_zalloc(sizeof(*py));
    return py;
}

void proxy_init(proxy_t *py);
void proxy_destroy(proxy_t *py);
int proxy_add(proxy_t *py, struct role *r);
void proxy_del(proxy_t *py, struct role *r);
void __proxy_walk(proxy_t *py, walkfn f, void *args, struct list_head *head);

#define proxy_walkrcver(py, f, args) __proxy_walk((py), (f), (args), (&py->rcver_head))
#define proxy_walksnder(py, f, args) __proxy_walk((py), (f), (args), (&py->snder_head))

struct role *__proxy_find(ssmap_t *map, uuid_t uuid);

static inline struct role *proxy_find_at(proxy_t *py, uuid_t uuid) {
    struct role *r;
    proxy_lock(py);
    r = __proxy_find(&py->roles, uuid);
    proxy_unlock(py);
    return r;
}

static inline struct role *proxy_find_tw(proxy_t *py, uuid_t uuid) {
    struct role *r;
    proxy_lock(py);
    r = __proxy_find(&py->tw_roles, uuid);
    proxy_unlock(py);
    return r;
}

struct role *proxy_lb_dispatch(proxy_t *py);


#endif
