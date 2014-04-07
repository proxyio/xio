#include <os/timesz.h>
#include <base.h>
#include "channel_base.h"

extern struct channel *cid_to_channel(int cd);

static struct upoll_entry *entry_new(struct upoll_event *event) {
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

static void event_notify(struct upoll_notify *un, struct upoll_entry *ent);

static struct upoll_table *upoll_new() {
    struct upoll_table *ut = (struct upoll_table *)mem_zalloc(sizeof(*ut));
    if (ut) {
	ut->notify.event = event_notify;
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

struct upoll_table *upoll_create() {
    struct upoll_table *ut = upoll_new();
    /* This one is hold by user caller and will be released after
     * user call upoll_close
     */
    if (ut)
	upoll_get(ut);
    return ut;
}

static struct upoll_entry *__find(struct upoll_table *ut, int cd) {
    struct upoll_entry *ent, *nx;

    /* TODO: we can use a rbtree make better performance */
    list_for_each_upoll_ent(ent, nx, &ut->lru_entries) {
	if (ent->event.cd == cd)
	    return ent;
    }
    return NULL;
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
    if ((ent = __find(ut, event->cd))) {
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

/* Remove a upoll_entry from upoll_table. The caller must hold the
 * upoll_table's lock */
static void __rm(struct upoll_table *ut, struct upoll_entry *ent) {
    spin_lock(&ent->lock);
    BUG_ON(!(ent->ref == 1 || ent->ref == 2));

    ut->tb_size--;
    ent->eflags |= UPOLLSTATE_DEL;
    list_del_init(&ent->lru_link);
    spin_unlock(&ent->lock);
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
    if (!(ent = __find(ut, event->cd))) {
	mutex_unlock(&ut->lock);
	errno = ENOENT;
	return -1;
    }
    __rm(ut, ent);
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
    if (!(ent = __find(ut, event->cd))) {
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
	__rm(ut, ent);
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
