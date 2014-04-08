#include <os/timesz.h>
#include <base.h>
#include "channel_base.h"

struct upoll_entry *entry_new(struct upoll_event *event) {
    struct upoll_entry *ent = (struct upoll_entry *)mem_zalloc(sizeof(*ent));
    if (ent) {
	INIT_LIST_HEAD(&ent->channel_link);
	INIT_LIST_HEAD(&ent->lru_link);
	spin_init(&ent->lock);
	ent->ref = 0;
	ent->eflags = UPOLLSTATE_NEW|UPOLLSTATE_DEL|UPOLLSTATE_CLOSED;
	ent->event = *event;
    }
    return ent;
}

static void entry_destroy(struct upoll_entry *ent) {
    struct upoll_table *ut = cont_of(ent->notify, struct upoll_table, notify);

    BUG_ON(ent->ref != 0);
    spin_destroy(&ent->lock);
    if (ent->notify)
	upoll_put(ut);
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

struct upoll_table *upoll_new() {
    struct upoll_table *ut = (struct upoll_table *)mem_zalloc(sizeof(*ut));
    if (ut) {
	ut->uwaiters = 0;
	ut->ref = 0;
	mutex_init(&ut->lock);
	INIT_LIST_HEAD(&ut->lru_entries);
    }
    return ut;
}

static void upoll_destroy(struct upoll_table *ut) {
    mutex_destroy(&ut->lock);
    mem_free(ut, sizeof(struct upoll_table));
}

int upoll_get(struct upoll_table *ut) {
    int ref;
    mutex_lock(&ut->lock);
    ref = ut->ref++;
    mutex_unlock(&ut->lock);
    return ref;
}

int upoll_put(struct upoll_table *ut) {
    int ref;
    mutex_lock(&ut->lock);
    ref = ut->ref--;
    BUG_ON(ut->ref < 0);
    mutex_unlock(&ut->lock);
    if (ref == 1)
	upoll_destroy(ut);
    return ref;
}
