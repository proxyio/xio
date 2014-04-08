#include <os/timesz.h>
#include <base.h>
#include "channel_base.h"

extern struct channel *cid_to_channel(int cd);

static void event_notify(struct upoll_notify *un, struct upoll_entry *ent);

struct upoll_table *upoll_create() {
    struct upoll_table *ut = upoll_new();
    /* This one is hold by user caller and will be released after
     * user call upoll_close
     */
    if (ut) {
	ut->notify.event = event_notify;	
	upoll_get(ut);
    }
    return ut;
}

/* Remove a upoll_entry from upoll_table. The caller must hold the
 * upoll_table's lock and entry's lock */
static void __rm(struct upoll_table *ut, struct upoll_entry *ent) {
    BUG_ON(!(ent->ref == 1 || ent->ref == 2));

    ut->tb_size--;
    ent->eflags |= UPOLLSTATE_DEL;
    list_del_init(&ent->lru_link);
}

/* Find an upoll_entry by channel id and remove it if channel is closed */
static struct upoll_entry *__find(struct upoll_table *ut, int cd, int lock) {
    struct upoll_entry *ent, *nx;
    list_for_each_upoll_ent(ent, nx, &ut->lru_entries) {
	spin_lock(&ent->lock);
	if (ent->eflags & UPOLLSTATE_CLOSED) {
	    __rm(ut, ent);
	    spin_unlock(&ent->lock);
	    continue;
	}
	if (ent->event.cd == cd) {
	    if (!lock)
		spin_unlock(&ent->lock);
	    return ent;
	}
	spin_unlock(&ent->lock);
    }
    return NULL;
}

static inline
struct upoll_entry *__find_no_lock(struct upoll_table *ut, int cd) {
    return __find(ut, cd, false);
}

static inline
struct upoll_entry *__find_and_lock(struct upoll_table *ut, int cd) {
    return __find(ut, cd, true);
}

/* The channel backend thread was remove the entry from channel table. */
static void event_notify(struct upoll_notify *un, struct upoll_entry *ent) {
    uint32_t happened = ent->event.happened;
    struct upoll_table *ut = cont_of(un, struct upoll_table, notify);
    
    /* In normal case we have three cases here.
     * case 1: ref == 1. upoll_rm this entry or upoll_add not done
     * case 2: ref == 2. normal state
     */
    mutex_lock(&ut->lock);
    spin_lock(&ent->lock);
    BUG_ON((ent->eflags & UPOLLSTATE_NEW));
    BUG_ON((ent->eflags & UPOLLSTATE_CLOSED));

    if ((ent->eflags & UPOLLSTATE_DEL)) {
	ent->eflags |= UPOLLSTATE_CLOSED;

	/* We can do this because of .. see check_upoll_events for details */
	list_del_init(&ent->channel_link);

	spin_unlock(&ent->lock);
	mutex_unlock(&ut->lock);
	/* Release the ref hold by channel */
	entry_put(ent);
	return;
    } else if (happened) {
	list_move(&ent->lru_link, &ut->lru_entries);
	if (ut->uwaiters)
	    condition_broadcast(&ut->cond);
    }
    spin_unlock(&ent->lock);
    mutex_unlock(&ut->lock);
}

static int upoll_add(struct upoll_table *ut, struct upoll_event *event) {
    int rc = 0;
    struct upoll_entry *ent;
    struct channel *cn = cid_to_channel(event->cd);
    
    mutex_lock(&ut->lock);
    /* Nobody know this entry at this time. so we doesn't lock it */
    /* Check exist entry */
    if ((ent = __find_no_lock(ut, event->cd))) {
	mutex_unlock(&ut->lock);
	errno = EEXIST;
	return -1;
    }
    if (!(ent = entry_new(event))) {
	mutex_unlock(&ut->lock);	
	errno = ENOMEM;
	return -1;
    }
    /* We hold it here because the two step are async. here guarantee
     * that this entry won't be destroy after we release the upoll_table's
     * lock. so
     * 1 for caller
     * 1 for upoll_table
     */
    ent->ref++;
    /* Cycle referencet of upoll_entry and upoll_table */
    ent->ref++;
    ut->ref++;
    ent->eflags &= ~UPOLLSTATE_DEL;

    /* step 1. insert entry into upoll table. */
    ut->tb_size++;
    ent->notify = &ut->notify;
    list_add_tail(&ent->lru_link, &ut->lru_entries);
    mutex_unlock(&ut->lock);

    /* Are there any asshole UPOLL_DEL the same entry in this
     * internal ? don't do so stupid thing ...
     */
    
    /* step 2. insert into channel tables */
    mutex_lock(&cn->lock);
    spin_lock(&ent->lock);

    /* Release the ref hold above and incr one for channel */
    ent->ref--;
    ent->ref++;
    ent->eflags &= ~UPOLLSTATE_CLOSED;

    /* clear UPOLLSTATE_NEW flag */
    ent->eflags &= ~UPOLLSTATE_NEW;
    list_add_tail(&ent->channel_link, &cn->upoll_head);
    spin_unlock(&ent->lock);
    mutex_unlock(&cn->lock);
    return rc;
}

/* WARNING: upoll_rm the same entry twice is a FATAL error */
static int upoll_rm(struct upoll_table *ut, struct upoll_event *event) {
    int rc = 0;
    struct upoll_entry *ent;

    /* In normal case we have two cases here.
     * case 1: ref == 1. channel closed
     * case 2: ref == 2. normal state or UPOLLSTATE_NEW
     */
    mutex_lock(&ut->lock);
    if (!(ent = __find_and_lock(ut, event->cd))) {
	mutex_unlock(&ut->lock);
	errno = ENOENT;
	return -1;
    }
    __rm(ut, ent);
    spin_unlock(&ent->lock);
    mutex_unlock(&ut->lock);

    /* Release the ref hold by upoll_table*/
    entry_put(ent);
    return rc;
}

static int upoll_mod(struct upoll_table *ut, struct upoll_event *event) {
    int rc = 0;
    struct upoll_entry *ent;

    /* In normal case we have two cases here.
     * case 1: ref == 1. channel closed
     * case 2: ref == 2. normal state or UPOLLSTATE_NEW
     */
    mutex_lock(&ut->lock);
    if (!(ent = __find_and_lock(ut, event->cd))) {
	mutex_unlock(&ut->lock);
	errno = ENOENT;
	return -1;
    }
    spin_lock(&ent->lock);
    BUG_ON(!(ent->ref == 1 || ent->ref == 2));

    /* Update upoll_event */
    ent->event.care = event->care;
    spin_unlock(&ent->lock);
    mutex_unlock(&ut->lock);
    return rc;
}

int upoll_ctl(struct upoll_table *ut, int op, struct upoll_event *event) {
    int rc = -1;

    switch (op) {
    case UPOLL_ADD:
	rc = upoll_add(ut, event);
	return rc;
    case UPOLL_DEL:
	rc = upoll_rm(ut, event);
	return rc;
    case UPOLL_MOD:
	rc = upoll_mod(ut, event);
	return rc;
    default:
	errno = EINVAL;
    }
    return -1;
}

int upoll_wait(struct upoll_table *ut, struct upoll_event *events,
	       int size, int timeout) {
    int n = 0;
    struct list_head head;
    struct upoll_entry *ent, *nx;

    mutex_lock(&ut->lock);
    INIT_LIST_HEAD(&head);

    /* If havn't any events here. we wait */
    ent = list_first(&ut->lru_entries, struct upoll_entry, lru_link);
    if (!ent->event.happened) {
	ut->uwaiters++;
	condition_wait(&ut->cond, &ut->lock);
	ut->uwaiters--;
    }
    list_for_each_upoll_ent(ent, nx, &ut->lru_entries) {
	if (!ent->event.happened || n >= size)
	    break;
	events[n++] = ent->event;
	ent->event.happened = 0;
	list_move(&ent->lru_link, &head);
    }
    list_splice(&ut->lru_entries, &head);
    list_splice(&head, &ut->lru_entries);
    mutex_unlock(&ut->lock);
    return n;
}


void upoll_close(struct upoll_table *ut) {
    struct upoll_entry *ent, *nx;
    struct list_head destroy_head;

    INIT_LIST_HEAD(&destroy_head);

    /* Remove all upoll_entry from upoll_table and then destroy them. */
    mutex_lock(&ut->lock);
    list_for_each_upoll_ent(ent, nx, &ut->lru_entries) {
	spin_lock(&ent->lock);
	__rm(ut, ent);
	spin_unlock(&ent->lock);
	list_add(&ent->lru_link, &destroy_head);
    }
    mutex_unlock(&ut->lock);

    list_for_each_upoll_ent(ent, nx, &destroy_head) {
	list_del_init(&ent->lru_link);
	entry_put(ent);
    }

    /* Release the ref hold by caller */
    upoll_put(ut);
}
