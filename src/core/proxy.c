#include "proxy.h"
#include "role.h"

void proxy_init(proxy_t *py) {
    ZERO(*py);
    spin_init(&(py)->lock);
    ssmap_init(&(py)->roles);
    ssmap_init(&(py)->tw_roles);
    INIT_LIST_HEAD(&(py)->rcver_head);
    INIT_LIST_HEAD(&(py)->snder_head);
}

void proxy_destroy(proxy_t *py) {
    struct role *r, *n;		

    spin_destroy(&(py)->lock);	
    list_for_each_role_safe(r, n, &(py)->rcver_head) {	
	r_destroy(r);					
	mem_free(r, sizeof(*r));			
    }							
    list_for_each_role_safe(r, n, &(py)->snder_head) {	
	r_destroy(r);					
	mem_free(r, sizeof(*r));			
    }							
}

int proxy_add(proxy_t *py, struct role *r) {
    int ret;
    struct list_head *__head;

    __head = IS_RCVER(r) ? &py->rcver_head : &py->snder_head;
    proxy_lock(py);
    if ((ret = ssmap_insert(&py->roles, &r->sibling)) == 0) {
	py->rsize++;
	list_add(&r->py_link, __head);
    }
    proxy_unlock(py);
    return ret;
}

void proxy_del(proxy_t *py, struct role *r) {
    proxy_lock(py);				
    py->rsize--;				
    ssmap_delete(&py->roles, &r->sibling);	
    list_del(&r->py_link);			
    proxy_unlock(py);			
}

void __proxy_walk(proxy_t *py, walkfn f, void *args, struct list_head *head) {
    struct role *r = NULL, *nxt = NULL;	
    proxy_lock(py);				
    list_for_each_role_safe(r, nxt, head)	
	f(py, r, args);			
    proxy_unlock(py);			
}

struct role *__proxy_find(ssmap_t *map, uuid_t uuid) {
    ssmap_node_t *node;
    struct role *r = NULL;
    if ((node = ssmap_find(map, (char *)uuid, sizeof(uuid_t))))
	r = container_of(node, struct role, sibling);
    return r;
}


struct role *proxy_lb_dispatch(proxy_t *py) {
    struct role *dest = NULL;
    if (!list_empty(&py->snder_head)) {
	dest = list_first(&py->snder_head, struct role, py_link);
	list_del(&dest->py_link);
	list_add_tail(&dest->py_link, &py->snder_head);
    }
    return dest;
}
