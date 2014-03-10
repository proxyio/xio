#ifndef _HPIO_GRP_
#define _HPIO_GRP_

#include "core.h"

typedef int (*walkfn) (grp_t *grp, struct role *r, void *args);

#define list_for_each_role_safe(pos, n, head)				\
    list_for_each_entry_safe(pos, n, head, struct role, grp_link)

#define grp_lock(grp) spin_lock(&grp->lock)
#define grp_unlock(grp) spin_unlock(&grp->lock)

static inline grp_t *grp_new() {
    grp_t *grp = (grp_t *)mem_zalloc(sizeof(*grp));
    return grp;
}

#define grp_init(grp) do {			\
	spin_init(&(grp)->lock);		\
	ssmap_init(&(grp)->roles);		\
	ssmap_init(&(grp)->tw_roles);		\
	INIT_LIST_HEAD(&(grp)->rcver_head);	\
	INIT_LIST_HEAD(&(grp)->snder_head);	\
    } while (0)

#define grp_destroy(grp) do {					\
	struct role *__r, *__n;					\
	spin_destroy(&(grp)->lock);				\
	list_for_each_role_safe(__r, __n, &(grp)->rcver_head) {	\
	    r_destroy(__r);					\
	    mem_free(__r, sizeof(*__r));			\
	}							\
	list_for_each_role_safe(__r, __n, &(grp)->snder_head) {	\
	    r_destroy(__r);					\
	    mem_free(__r, sizeof(*__r));			\
	}							\
    } while (0)

#define grp_add(grp, r) do {						\
	struct list_head *__head = NULL;				\
	__head = IS_RCVER(r) ? &grp->rcver_head : &grp->snder_head;	\
	grp_lock(grp);							\
	grp->rsize++;							\
	ssmap_insert(&grp->roles, &r->sibling);				\
	list_add(&r->grp_link, __head);					\
	grp_unlock(grp);						\
    } while (0)

#define grp_del(grp, r) do {			\
	grp_lock(grp);				\
	ssmap_delete(&grp->roles, &r->sibling);	\
	list_del(&r->grp_link);			\
	grp->rsize--;				\
	grp_unlock(grp);			\
    } while (0)

#define __grp_walk(grp, f, args, head) do {	\
	struct role *r = NULL, *nxt = NULL;	\
	grp_lock(grp);				\
	list_for_each_role_safe(r, nxt, head)	\
	    f(grp, r, args);			\
	grp_unlock(grp);			\
    } while (0)

#define grp_walkrcver(grp, f, args) __grp_walk(grp, f, args, &grp->rcver_head)
#define grp_walksnder(grp, f, args) __grp_walk(grp, f, args, &grp->snder_head)


static inline struct role *__grp_find(grp_t *grp, uuid_t uuid, ssmap_t *map) {
    ssmap_node_t *node;
    struct role *r = NULL;

    grp_lock(grp);
    if (!(node = ssmap_find(map, (char *)uuid, sizeof(uuid))))
	r = container_of(node, struct role, sibling);
    grp_unlock(grp);
    return r;
}
#define grp_find(grp, uuid) __grp_find(grp, uuid, &grp->roles)
#define grp_find_tw(grp, uuid) __grp_find(grp, uuid, &grp->tw_roles)

static inline struct role *grp_loadbalance_dest(grp_t *grp) {
    return NULL;
}


#endif
