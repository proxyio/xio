#include "proxy.h"
#include "role.h"


#define list_for_each_role_safe(pos, n, head)				\
    list_for_each_entry_safe(pos, n, head, struct role, py_link)

void proxy_init(proxy_t *py) {
    ZERO(*py);
    spin_init(&(py)->lock);
    ssmap_init(&(py)->roles);
    INIT_LIST_HEAD(&(py)->rcver_head);
    INIT_LIST_HEAD(&(py)->snder_head);
}

void proxy_destroy(proxy_t *py) {
    struct list_head head = {};
    struct role *r, *n;		

    spin_destroy(&(py)->lock);
    INIT_LIST_HEAD(&head);
    list_splice(&py->rcver_head, &head);
    list_splice(&py->snder_head, &head);
    list_for_each_role_safe(r, n, &head) {
	list_del(&r->py_link);
	eloop_del(r->el, &r->et);
	close(r->et.fd);
	r_destroy(r);					
	mem_free(r, sizeof(*r));			
    }							
}

int proxy_add(proxy_t *py, struct role *r) {
    int rc;
    struct list_head *__head;

    __head = IS_RCVER(r) ? &py->rcver_head : &py->snder_head;
    lock(py);
    if ((rc = ssmap_insert(&py->roles, &r->sibling)) == 0) {
	py->rsize++;
	list_add(&r->py_link, __head);
    }
    unlock(py);
    return rc;
}

void proxy_del(proxy_t *py, struct role *r) {
    lock(py);				
    py->rsize--;				
    ssmap_delete(&py->roles, &r->sibling);	
    list_del(&r->py_link);			
    unlock(py);			
}

void proxy_walkr(proxy_t *py, walkfn f, void *args, int flags) {
    struct role *r = NULL, *nxt = NULL;	
    lock(py);
    if ((flags & PIO_RCVER))
	list_for_each_role_safe(r, nxt, &py->rcver_head)
	    f(py, r, args);
    if ((flags & PIO_SNDER))
	list_for_each_role_safe(r, nxt, &py->snder_head)
	    f(py, r, args);
    unlock(py);
}

struct role *proxy_getr(proxy_t *py, uuid_t uuid) {
    ssmap_node_t *node;
    struct role *r = NULL;
    lock(py);
    if ((node = ssmap_find(&py->roles, (char *)uuid, sizeof(uuid_t)))) {
	r = container_of(node, struct role, sibling);
	atomic_inc(&r->ref);
    }
    unlock(py);
    return r;
}


struct role *proxy_lb_dispatch(proxy_t *py) {
    struct role *dest = NULL;
    lock(py);
    if (!list_empty(&py->snder_head)) {
	dest = list_first(&py->snder_head, struct role, py_link);
	list_del(&dest->py_link);
	list_add_tail(&dest->py_link, &py->snder_head);
    }
    unlock(py);
    return dest;
}
