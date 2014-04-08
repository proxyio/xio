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
	ent->eflags = UPOLLSTATE_NEW|UPOLLSTATE_DEL|UPOLLSTATE_CLOSED;
    }
    return ent;
}

static void entry_destroy(struct upoll_entry *ent) {
    BUG_ON(ent->ref != 0);
    spin_destroy(&ent->lock);
    mem_free(ent, sizeof(*ent));
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

struct upoll_tb *tb_new() {
    struct upoll_tb *tb = (struct upoll_tb *)mem_zalloc(sizeof(*tb));
    if (tb) {
	tb->uwaiters = 0;
	tb->ref = 0;
	tb->size = 0;
	mutex_init(&tb->lock);
	condition_init(&tb->cond);
	INIT_LIST_HEAD(&tb->lru_head);
    }
    return tb;
}

static void tb_destroy(struct upoll_tb *tb) {
    mutex_destroy(&tb->lock);
    mem_free(tb, sizeof(struct upoll_tb));
}

int tb_get(struct upoll_tb *tb) {
    int ref;
    mutex_lock(&tb->lock);
    ref = tb->ref++;
    mutex_unlock(&tb->lock);
    return ref;
}

int tb_put(struct upoll_tb *tb) {
    int ref;
    mutex_lock(&tb->lock);
    ref = tb->ref--;
    BUG_ON(tb->ref < 0);
    mutex_unlock(&tb->lock);
    if (ref == 1)
	tb_destroy(tb);
    return ref;
}

struct upoll_entry *__tb_find(struct upoll_tb *tb, int cd) {
    struct upoll_entry *ent, *nx;
    list_for_each_upoll_ent(ent, nx, &tb->lru_head) {
	if (ent->event.cd == cd)
	    return ent;
    }
    return NULL;
}

/* Find upoll_entry by channel id and return with ref incr if exist. */
struct upoll_entry *tb_find(struct upoll_tb *tb, int cd) {
    struct upoll_entry *ent = NULL;
    mutex_lock(&tb->lock);
    if ((ent = __tb_find(tb, cd)))
	entry_get(ent);
    mutex_unlock(&tb->lock);
    return ent;
}


void __entry_attach_to_tb(struct upoll_entry *ent, struct upoll_tb *tb) {
    BUG_ON(attached(&ent->lru_link));
    tb->size++;
    list_add_tail(&ent->lru_link, &tb->lru_head);
}

void entry_attach_to_tb(struct upoll_entry *ent, struct upoll_tb *tb) {
    mutex_lock(&tb->lock);
    __entry_attach_to_tb(ent, tb);
    mutex_unlock(&tb->lock);
}

void __entry_detach_from_tb(struct upoll_entry *ent, struct upoll_tb *tb) {
    BUG_ON(!attached(&ent->lru_link));
    tb->size--;
    list_del_init(&ent->lru_link);
}

void entry_detach_from_tb(struct upoll_entry *ent, struct upoll_tb *tb) {
    mutex_lock(&tb->lock);
    __entry_detach_from_tb(ent, tb);
    mutex_unlock(&tb->lock);
}

/* Create a new upoll_entry if the cd doesn't exist and get one ref for
 * caller. upoll_add() call this.
 */
struct upoll_entry *tb_getent(struct upoll_tb *tb, int cd) {
    struct upoll_entry *ent;

    mutex_lock(&tb->lock);
    if ((ent = __tb_find(tb, cd))) {
	mutex_unlock(&tb->lock);
	errno = EEXIST;
	return NULL;
    }
    if (!(ent = entry_new())) {
	mutex_unlock(&tb->lock);
	errno = ENOMEM;
	return NULL;
    }

    /* One reference for back for caller */
    ent->ref++;

    /* Cycle reference of upoll_tb and upoll_entry */
    ent->ref++;
    tb->ref++;

    ent->event.cd = cd;
    __entry_attach_to_tb(ent, tb);
    mutex_unlock(&tb->lock);

    return ent;
}

/* Remove the upoll_entry if the cd's ent exist. notice that don't release the
 * ref hold by upoll_tb. let caller do this.
 * upoll_rm() call this.
 */
struct upoll_entry *tb_putent(struct upoll_tb *tb, int cd) {
    struct upoll_entry *ent;

    mutex_lock(&tb->lock);
    if (!(ent = __tb_find(tb, cd))) {
	mutex_unlock(&tb->lock);
	errno = ENOENT;
	return NULL;
    }
    __entry_detach_from_tb(ent, tb);
    mutex_unlock(&tb->lock);

    /* Release the upoll_tb's ref hold by upoll_entry */
    tb_put(tb);
    return ent;
}


/* Pop the first upoll_entry. notice that don't release the ref hold
 * by upoll_tb. let caller do this.
 * upoll_close() call this.
 */
struct upoll_entry *tb_popent(struct upoll_tb *tb) {
    struct upoll_entry *ent;

    mutex_lock(&tb->lock);
    if (list_empty(&tb->lru_head)) {
	mutex_unlock(&tb->lock);
	errno = ENOENT;
	return NULL;
    }
    ent = list_first(&tb->lru_head, struct upoll_entry, lru_link);
    __entry_detach_from_tb(ent, tb);
    mutex_unlock(&tb->lock);

    /* Release the upoll_tb's ref hold by upoll_entry. remember that this
     * is an cycle ref
     */
    tb_put(tb);
    return ent;
}


void entry_attach_to_channel(struct upoll_entry *ent, int cd) {
    struct channel *cn = cid_to_channel(cd);

    mutex_lock(&cn->lock);
    /* See the BUG case 1 of upoll_rm for more details */
    BUG_ON(attached(&ent->channel_link));
    list_add_tail(&ent->channel_link, &cn->upoll_head);
    mutex_unlock(&cn->lock);
}


void entry_detach_from_channel(struct upoll_entry *ent, int cd) {
    struct channel *cn = cid_to_channel(cd);

    mutex_lock(&cn->lock);

    /* See the BUG case 1 of upoll_rm for more details */
    if (attached(&ent->channel_link))
	list_del_init(&ent->channel_link);
    mutex_unlock(&cn->lock);
}
