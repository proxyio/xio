#include <os/timesz.h>
#include <base.h>
#include "channel_base.h"

extern struct channel *cid_to_channel(int cd);

static void event_notify(struct upoll_notify *un, struct upoll_entry *ent);

struct upoll_tb *upoll_create() {
    struct upoll_tb *tb = tb_new();

    if (tb) {
	tb->notify.event = event_notify;
	tb_get(tb);
    }
    return tb;
}

static void event_notify(struct upoll_notify *un, struct upoll_entry *ent) {
    struct upoll_tb *tb = cont_of(un, struct upoll_tb, notify);

    mutex_lock(&tb->lock);
    BUG_ON(!ent->event.care);
    if (!attached(&ent->lru_link)) {
	__detach_from_channel(ent);
	mutex_unlock(&tb->lock);
	entry_put(ent);
	return;
    }
    spin_lock(&ent->lock);
    if (ent->event.happened && (ent->event.care & ent->event.happened)) {
	list_move(&ent->lru_link, &tb->lru_head);
	if (tb->uwaiters)
	    condition_broadcast(&tb->cond);
    }
    spin_unlock(&ent->lock);
    mutex_unlock(&tb->lock);
}

static int upoll_add(struct upoll_tb *tb, struct upoll_event *event) {
    struct upoll_entry *ent = tb_getent(tb, event->cd);
    struct channel *cn = cid_to_channel(event->cd);

    if (!ent)
	return -1;

    /* Set up events callback */
    spin_lock(&ent->lock);
    ent->event = *event;
    ent->notify = &tb->notify;
    spin_unlock(&ent->lock);

    /* We hold a ref here. it is used for channel */
    /* BUG case 1: it's possible that this entry was deleted by upoll_rm() */
    attach_to_channel(ent, cn->cd);
    return 0;
}

static int upoll_rm(struct upoll_tb *tb, struct upoll_event *event) {
    struct upoll_entry *ent = tb_putent(tb, event->cd);

    if (!ent)
	return -1;

    /* Release the ref hold by upoll_tb */
    entry_put(ent);
    return 0;
}

static int upoll_mod(struct upoll_tb *tb, struct upoll_event *event) {
    struct upoll_entry *ent = tb_find(tb, event->cd);

    if (!ent)
	return -1;
    mutex_lock(&tb->lock);
    spin_lock(&ent->lock);
    ent->event.care = event->care;

    /* If we mod the care events. we should notice user's backend
     * poll thread.
     */
    ent->event.happened = ent->event.care;
    list_move(&ent->lru_link, &tb->lru_head);
    if (tb->uwaiters)
	condition_broadcast(&tb->cond);
    spin_unlock(&ent->lock);
    mutex_unlock(&tb->lock);
    
    /* Release the ref hold by caller */
    entry_put(ent);
    return 0;
}

int upoll_ctl(struct upoll_tb *tb, int op, struct upoll_event *event) {
    int rc = -1;

    switch (op) {
    case UPOLL_ADD:
	rc = upoll_add(tb, event);
	return rc;
    case UPOLL_DEL:
	rc = upoll_rm(tb, event);
	return rc;
    case UPOLL_MOD:
	rc = upoll_mod(tb, event);
	return rc;
    default:
	errno = EINVAL;
    }
    return -1;
}

int upoll_wait(struct upoll_tb *tb, struct upoll_event *events,
	       int size, u32 timeout) {
    int n = 0;
    struct list_head head;
    struct upoll_entry *ent, *nx;

    mutex_lock(&tb->lock);
    INIT_LIST_HEAD(&head);

    /* If havn't any events here. we wait */
    ent = list_first(&tb->lru_head, struct upoll_entry, lru_link);
    if (!ent->event.happened && timeout > 0) {
	tb->uwaiters++;
	condition_timedwait(&tb->cond, &tb->lock, timeout);
	tb->uwaiters--;
    }
    list_for_each_upoll_ent(ent, nx, &tb->lru_head) {
	if (!ent->event.happened || n >= size)
	    break;
	events[n++] = ent->event;
	ent->event.happened = 0;
	list_move(&ent->lru_link, &head);
    }
    list_splice(&tb->lru_head, &head);
    list_splice(&head, &tb->lru_head);
    mutex_unlock(&tb->lock);
    return n;
}


void upoll_close(struct upoll_tb *tb) {
    struct upoll_entry *ent;

    while ((ent = tb_popent(tb))) {
	/* Release the ref hold by upoll_tb */
	entry_put(ent);
    }
    /* Release the ref hold by user-caller */
    tb_put(tb);
}
