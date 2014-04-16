#include <os/timesz.h>
#include <base.h>
#include "xbase.h"

struct xpoll_entry *xent_new() {
    struct xpoll_entry *ent = (struct xpoll_entry *)mem_zalloc(sizeof(*ent));
    if (ent) {
	INIT_LIST_HEAD(&ent->xlink);
	INIT_LIST_HEAD(&ent->lru_link);
	spin_init(&ent->lock);
	ent->ref = 0;
    }
    return ent;
}

int xpoll_put(struct xpoll_t *po);

static void xent_destroy(struct xpoll_entry *ent) {
    struct xpoll_t *po = cont_of(ent->notify, struct xpoll_t, notify);

    BUG_ON(ent->ref != 0);
    spin_destroy(&ent->lock);
    mem_free(ent, sizeof(*ent));
    xpoll_put(po);
}


int xent_get(struct xpoll_entry *ent) {
    int ref;
    spin_lock(&ent->lock);
    ref = ent->ref++;
    /* open for debuging
       if (ent->i_idx < sizeof(ent->incr_tid))
       ent->incr_tid[ent->i_idx++] = gettid();
    */
    spin_unlock(&ent->lock);
    return ref;
}

int xent_put(struct xpoll_entry *ent) {
    int ref;

    spin_lock(&ent->lock);
    ref = ent->ref--;
    BUG_ON(ent->ref < 0);
    /* open for debuging
       if (ent->d_idx < sizeof(ent->desc_tid))
       ent->desc_tid[ent->d_idx++] = gettid();
    */
    spin_unlock(&ent->lock);
    if (ref == 1) {
	BUG_ON(attached(&ent->lru_link));
	BUG_ON(attached(&ent->xlink));
	xent_destroy(ent);
    }
    return ref;
}

struct xpoll_t *xpoll_new() {
    struct xpoll_t *po = (struct xpoll_t *)mem_zalloc(sizeof(*po));
    if (po) {
	po->uwaiters = 0;
	po->ref = 0;
	po->size = 0;
	mutex_init(&po->lock);
	condition_init(&po->cond);
	INIT_LIST_HEAD(&po->lru_head);
    }
    return po;
}

static void xpoll_destroy(struct xpoll_t *po) {
    mutex_destroy(&po->lock);
    mem_free(po, sizeof(struct xpoll_t));
}

int xpoll_get(struct xpoll_t *po) {
    int ref;
    mutex_lock(&po->lock);
    ref = po->ref++;
    mutex_unlock(&po->lock);
    return ref;
}

int xpoll_put(struct xpoll_t *po) {
    int ref;
    mutex_lock(&po->lock);
    ref = po->ref--;
    BUG_ON(po->ref < 0);
    mutex_unlock(&po->lock);
    if (ref == 1)
	xpoll_destroy(po);
    return ref;
}

struct xpoll_entry *__xpoll_find(struct xpoll_t *po, int xd) {
    struct xpoll_entry *ent, *nx;
    xpoll_walk_ent(ent, nx, &po->lru_head) {
	if (ent->event.xd == xd)
	    return ent;
    }
    return NULL;
}

/* Find xpoll_entry by xsock id and return with ref incr if exist. */
struct xpoll_entry *xpoll_find(struct xpoll_t *po, int xd) {
    struct xpoll_entry *ent = NULL;
    mutex_lock(&po->lock);
    if ((ent = __xpoll_find(po, xd)))
	xent_get(ent);
    mutex_unlock(&po->lock);
    return ent;
}


void __attach_to_po(struct xpoll_entry *ent, struct xpoll_t *po) {
    BUG_ON(attached(&ent->lru_link));
    po->size++;
    list_add_tail(&ent->lru_link, &po->lru_head);
}

void __detach_from_po(struct xpoll_entry *ent, struct xpoll_t *po) {
    BUG_ON(!attached(&ent->lru_link));
    po->size--;
    list_del_init(&ent->lru_link);
}


/* Create a new xpoll_entry if the xd doesn't exist and get one ref for
 * caller. xpoll_add() call this.
 */
struct xpoll_entry *xpoll_getent(struct xpoll_t *po, int xd) {
    struct xpoll_entry *ent;

    mutex_lock(&po->lock);
    if ((ent = __xpoll_find(po, xd))) {
	mutex_unlock(&po->lock);
	errno = EEXIST;
	return NULL;
    }
    if (!(ent = xent_new())) {
	mutex_unlock(&po->lock);
	errno = ENOMEM;
	return NULL;
    }

    /* One reference for back for caller */
    ent->ref++;

    /* Cycle reference of xpoll_t and xpoll_entry */
    ent->ref++;
    po->ref++;

    ent->event.xd = xd;
    __attach_to_po(ent, po);
    mutex_unlock(&po->lock);

    return ent;
}

/* Remove the xpoll_entry if the xd's ent exist. notice that don't release the
 * ref hold by xpoll_t. let caller do this.
 * xpoll_rm() call this.
 */
struct xpoll_entry *xpoll_putent(struct xpoll_t *po, int xd) {
    struct xpoll_entry *ent;

    mutex_lock(&po->lock);
    if (!(ent = __xpoll_find(po, xd))) {
	mutex_unlock(&po->lock);
	errno = ENOENT;
	return NULL;
    }
    __detach_from_po(ent, po);
    mutex_unlock(&po->lock);

    /* Release the xpoll_t's ref hold by xpoll_entry
     * xpoll_put(po);
     */
    return ent;
}


/* Pop the first xpoll_entry. notice that don't release the ref hold
 * by xpoll_t. let caller do this.
 * xpoll_close() call this.
 */
struct xpoll_entry *xpoll_popent(struct xpoll_t *po) {
    struct xpoll_entry *ent;

    mutex_lock(&po->lock);
    if (list_empty(&po->lru_head)) {
	mutex_unlock(&po->lock);
	errno = ENOENT;
	return NULL;
    }
    ent = list_first(&po->lru_head, struct xpoll_entry, lru_link);
    __detach_from_po(ent, po);
    mutex_unlock(&po->lock);

    /* Release the xpoll_t's ref hold by xpoll_entry. remember that this
     * is an cycle ref
     * xpoll_put(po);
     */
    return ent;
}


void attach_to_xsock(struct xpoll_entry *ent, int xd) {
    struct xsock *cn = xget(xd);

    mutex_lock(&cn->lock);
    BUG_ON(attached(&ent->xlink));
    list_add_tail(&ent->xlink, &cn->xpoll_head);
    mutex_unlock(&cn->lock);
}


void __detach_from_xsock(struct xpoll_entry *ent) {
    BUG_ON(!attached(&ent->xlink));
    list_del_init(&ent->xlink);
}
