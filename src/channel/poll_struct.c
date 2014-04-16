#include <os/timesz.h>
#include <base.h>
#include "channel_base.h"

extern struct channel *cid_to_channel(int cd);

struct upoll_entry *entry_new() {
    struct upoll_entry *ent = (struct upoll_entry *)mem_zalloc(sizeof(*ent));
    if (ent) {
	INIT_LIST_HEAD(&ent->channel_link);
	INIT_LIST_HEAD(&ent->lru_link);
	spin_init(&ent->lock);
	ent->ref = 0;
    }
    return ent;
}

int po_put(struct upoll_t *po);

static void entry_destroy(struct upoll_entry *ent) {
    struct upoll_t *po = cont_of(ent->notify, struct upoll_t, notify);

    BUG_ON(ent->ref != 0);
    spin_destroy(&ent->lock);
    mem_free(ent, sizeof(*ent));
    po_put(po);
}


int entry_get(struct upoll_entry *ent) {
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

int entry_put(struct upoll_entry *ent) {
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
	BUG_ON(attached(&ent->channel_link));
	entry_destroy(ent);
    }
    return ref;
}

struct upoll_t *po_new() {
    struct upoll_t *po = (struct upoll_t *)mem_zalloc(sizeof(*po));
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

static void po_destroy(struct upoll_t *po) {
    mutex_destroy(&po->lock);
    mem_free(po, sizeof(struct upoll_t));
}

int po_get(struct upoll_t *po) {
    int ref;
    mutex_lock(&po->lock);
    ref = po->ref++;
    mutex_unlock(&po->lock);
    return ref;
}

int po_put(struct upoll_t *po) {
    int ref;
    mutex_lock(&po->lock);
    ref = po->ref--;
    BUG_ON(po->ref < 0);
    mutex_unlock(&po->lock);
    if (ref == 1)
	po_destroy(po);
    return ref;
}

struct upoll_entry *__po_find(struct upoll_t *po, int cd) {
    struct upoll_entry *ent, *nx;
    list_for_each_upoll_ent(ent, nx, &po->lru_head) {
	if (ent->event.cd == cd)
	    return ent;
    }
    return NULL;
}

/* Find upoll_entry by channel id and return with ref incr if exist. */
struct upoll_entry *po_find(struct upoll_t *po, int cd) {
    struct upoll_entry *ent = NULL;
    mutex_lock(&po->lock);
    if ((ent = __po_find(po, cd)))
	entry_get(ent);
    mutex_unlock(&po->lock);
    return ent;
}


void __attach_to_po(struct upoll_entry *ent, struct upoll_t *po) {
    BUG_ON(attached(&ent->lru_link));
    po->size++;
    list_add_tail(&ent->lru_link, &po->lru_head);
}

void __detach_from_po(struct upoll_entry *ent, struct upoll_t *po) {
    BUG_ON(!attached(&ent->lru_link));
    po->size--;
    list_del_init(&ent->lru_link);
}


/* Create a new upoll_entry if the cd doesn't exist and get one ref for
 * caller. upoll_add() call this.
 */
struct upoll_entry *po_getent(struct upoll_t *po, int cd) {
    struct upoll_entry *ent;

    mutex_lock(&po->lock);
    if ((ent = __po_find(po, cd))) {
	mutex_unlock(&po->lock);
	errno = EEXIST;
	return NULL;
    }
    if (!(ent = entry_new())) {
	mutex_unlock(&po->lock);
	errno = ENOMEM;
	return NULL;
    }

    /* One reference for back for caller */
    ent->ref++;

    /* Cycle reference of upoll_t and upoll_entry */
    ent->ref++;
    po->ref++;

    ent->event.cd = cd;
    __attach_to_po(ent, po);
    mutex_unlock(&po->lock);

    return ent;
}

/* Remove the upoll_entry if the cd's ent exist. notice that don't release the
 * ref hold by upoll_t. let caller do this.
 * upoll_rm() call this.
 */
struct upoll_entry *po_putent(struct upoll_t *po, int cd) {
    struct upoll_entry *ent;

    mutex_lock(&po->lock);
    if (!(ent = __po_find(po, cd))) {
	mutex_unlock(&po->lock);
	errno = ENOENT;
	return NULL;
    }
    __detach_from_po(ent, po);
    mutex_unlock(&po->lock);

    /* Release the upoll_t's ref hold by upoll_entry
     * po_put(po);
     */
    return ent;
}


/* Pop the first upoll_entry. notice that don't release the ref hold
 * by upoll_t. let caller do this.
 * upoll_close() call this.
 */
struct upoll_entry *po_popent(struct upoll_t *po) {
    struct upoll_entry *ent;

    mutex_lock(&po->lock);
    if (list_empty(&po->lru_head)) {
	mutex_unlock(&po->lock);
	errno = ENOENT;
	return NULL;
    }
    ent = list_first(&po->lru_head, struct upoll_entry, lru_link);
    __detach_from_po(ent, po);
    mutex_unlock(&po->lock);

    /* Release the upoll_t's ref hold by upoll_entry. remember that this
     * is an cycle ref
     * po_put(po);
     */
    return ent;
}


void attach_to_channel(struct upoll_entry *ent, int cd) {
    struct channel *cn = cid_to_channel(cd);

    mutex_lock(&cn->lock);
    BUG_ON(attached(&ent->channel_link));
    list_add_tail(&ent->channel_link, &cn->upoll_head);
    mutex_unlock(&cn->lock);
}


void __detach_from_channel(struct upoll_entry *ent) {
    BUG_ON(!attached(&ent->channel_link));
    list_del_init(&ent->channel_link);
}
