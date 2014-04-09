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
    if (ent->event.happened) {
	list_move(&ent->lru_link, &tb->lru_head);
	if (tb->uwaiters)
	    condition_broadcast(&tb->cond);
    }
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

    /* Incr one ref for caller before other thread can touch it. */
    entry_get(ent);

    /* We hold a ref here. it is used for channel */
    /* BUG case 1: it's possible that this entry was deleted by upoll_rm() */
    entry_attach_to_channel(ent, cn->cd);

    /* Release caller's ref after work done. */
    entry_put(ent);
    return 0;
}

/* WARNING: upoll_rm the same entry twice is a FATAL error */
static int upoll_rm(struct upoll_tb *tb, struct upoll_event *event) {
    struct upoll_entry *ent = tb_putent(tb, event->cd);

    if (!ent)
	return -1;

    /* BUG case 1:
     * maybe the upoll_add not done. ent only attached to upoll_tb
     * haven't attached to channel yet. event_notify process the case.
     */
    entry_detach_from_channel(ent, event->cd);

    /* Release the ref hold by upoll_tb and channel */
    entry_put(ent);
    entry_put(ent);
    return 0;
}

static int upoll_mod(struct upoll_tb *tb, struct upoll_event *event) {
    struct upoll_entry *ent = tb_find(tb, event->cd);

    if (!ent)
	return -1;
    spin_lock(&ent->lock);
    ent->event.care = event->care;
    spin_unlock(&ent->lock);

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
	       int size, int timeout) {
    int n = 0;
    struct list_head head;
    struct upoll_entry *ent, *nx;

    mutex_lock(&tb->lock);
    INIT_LIST_HEAD(&head);

    /* If havn't any events here. we wait */
    ent = list_first(&tb->lru_head, struct upoll_entry, lru_link);
    if (!ent->event.happened && timeout != 0) {
	tb->uwaiters++;
	condition_wait(&tb->cond, &tb->lock);
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
    struct list_head lru_head;
    struct upoll_entry *ent, *nx;

    INIT_LIST_HEAD(&lru_head);
    mutex_lock(&tb->lock);
    list_splice(&tb->lru_head, &lru_head);
    mutex_unlock(&tb->lock);

    list_for_each_upoll_ent(ent, nx, &lru_head)
	upoll_ctl(tb, UPOLL_DEL, &ent->event);

    /* Release the ref hold by user-caller */
    tb_put(tb);
}
