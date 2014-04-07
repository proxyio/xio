#include <os/timesz.h>
#include <base.h>
#include "channel_base.h"

extern struct channel *cid_to_channel(int cd);
extern struct channel_poll *pid_to_channel_poll(int pd);

int has_closed_channel(struct channel_poll *po) {
    int has = true;
    spin_lock(&po->lock);
    if (list_empty(&po->closing_head))
	has = false;
    spin_unlock(&po->lock);
    return has;
}

void push_closed_channel(struct channel *cn) {
    struct channel_poll *po = pid_to_channel_poll(cn->pollid);
    
    spin_lock(&po->lock);
    if (!cn->fclosed && !attached(&cn->closing_link)) {
	cn->fclosed = true;
	list_add_tail(&cn->closing_link, &po->closing_head);
    }
    spin_unlock(&po->lock);
}

struct channel *pop_closed_channel(struct channel_poll *po) {
    struct channel *cn = NULL;

    spin_lock(&po->lock);
    if (!list_empty(&po->closing_head)) {
	cn = list_first(&po->closing_head, struct channel, closing_link);
	list_del_init(&cn->closing_link);
    }
    spin_unlock(&po->lock);
    return cn;
}



/* The channel kernel will hold the channel's lock when call this */
static void uupoll_event(struct upoll_notify *un, struct upoll_entry *ent) {
    uint32_t happened = ent->pev.events_happened;
    struct upoll_table *ut = cont_of(un, struct upoll_table, notify);

    /* If any happened, insert it into happened_entries list */
    mutex_lock(&ut->lock);
    if (happened && !attached(&ent->happened_link)) {
	list_add_tail(&ent->happened_link, &ut->happened_entries);
	if (ut->uwaiters)
	    condition_broadcast(&ut->cond);
    }
    mutex_unlock(&ut->lock);
}


struct upoll_table *upoll_create() {
    struct upoll_table *ut = (struct upoll_table *)mem_zalloc(sizeof(*ut));
    if (ut) {
	ut->notify.event = uupoll_event;
	ut->uwaiters = 0;
	mutex_init(&ut->lock);
	ut->cur_happened_events = 0;
	INIT_LIST_HEAD(&ut->happened_entries);
	INIT_LIST_HEAD(&ut->poll_entries);
    }
    return ut;
}

static struct upoll_entry *upoll_find(struct upoll_table *ut, int cd) {
    struct upoll_entry *ent = NULL, *pos, *nx;

    mutex_lock(&ut->lock);
    /* TODO: we can use a rbtree make better performance */
    list_for_each_upollentry(pos, nx, &ut->poll_entries) {
	if (pos->pev.cd == cd) {
	    ent = pos;
	    break;
	}
    }
    mutex_unlock(&ut->lock);
    return ent;
}

static int upoll_add(struct upoll_table *ut, struct upoll_event *pev) {
    int rc = -1;
    struct upoll_entry *ent = (struct upoll_entry *)mem_zalloc(sizeof(*ent));    
    struct channel *cn = cid_to_channel(pev->cd);

    if (ent) {
	INIT_LIST_HEAD(&ent->channel_link);
	INIT_LIST_HEAD(&ent->polltable_link);
	INIT_LIST_HEAD(&ent->happened_link);
	ent->pev = *pev;
	ent->notify = &ut->notify;
	mutex_lock(&ut->lock);
	ut->ent_size++;
	list_add_tail(&ent->polltable_link, &ut->poll_entries);

	mutex_lock(&cn->lock);
	list_add_tail(&ent->channel_link, &cn->upoll_entries);
	mutex_unlock(&cn->lock);

	mutex_unlock(&ut->lock);
	return rc;
    }
    return rc;
}


static int upoll_rm(struct upoll_table *ut, struct upoll_event *pev) {
    int rc = 0;
    struct upoll_entry *ent = upoll_find(ut, pev->cd);
    struct channel *cn = cid_to_channel(pev->cd);
    
    mutex_lock(&ut->lock);
    ut->ent_size--;
    list_del_init(&ent->polltable_link);
    mutex_lock(&cn->lock);
    list_del_init(&ent->channel_link);
    mutex_unlock(&cn->lock);
    mutex_unlock(&ut->lock);
    return rc;
}

static int upoll_mod(struct upoll_table *ut, struct upoll_event *pev) {
    int rc = 0;
    struct upoll_entry *ent = upoll_find(ut, pev->cd);

    mutex_lock(&ut->lock);
    ent->pev.events = pev->events;
    mutex_unlock(&ut->lock);
    return rc;
}

int upoll_ctl(struct upoll_table *ut, int op, struct upoll_event *pev) {
    int rc = -1;

    switch (op) {
    case UPOLL_ADD:
	rc = upoll_add(ut, pev);
	break;
    case UPOLL_DEL:
	rc = upoll_rm(ut, pev);
	break;
    case UPOLL_MOD:
	rc = upoll_mod(ut, pev);
	break;
    default:
	errno = EINVAL;
    }
    return -1;
}

int upoll_wait(struct upoll_table *ut, struct upoll_event *vec,
	       int vec_sz, int timeout) {
    int n = 0;
    int64_t end = rt_mstime() + timeout;
    struct upoll_event *cn = vec;
    struct upoll_entry *ent, *nx;
    
    mutex_lock(&ut->lock);
    while (ut->cur_happened_events < 0 && rt_mstime() < end) {
	ut->uwaiters++;
	condition_wait(&ut->cond, &ut->lock);
	ut->uwaiters--;
    }
    if (ut->cur_happened_events > 0) {
	if (vec_sz > ut->cur_happened_events)
	    vec_sz = ut->cur_happened_events;
	n = vec_sz;
	list_for_each_upollentry(ent, nx, &ut->happened_entries) {
	    if (vec_sz <= 0)
		break;
	    BUG_ON(!ent->pev.events_happened);
	    list_del_init(&ent->happened_link);

	    /* Copy events detail into user-space buf and then clear */
	    *vec = ent->pev;
	    ent->pev.events_happened = 0;
	    vec_sz--;
	    vec++;
	}
	BUG_ON(vec - cn != n);
    }
    mutex_unlock(&ut->lock);
    return n;
}

void upoll_close(struct upoll_table *ut) {
    struct upoll_entry *ent, *nx;

    /* TODO: BUGON here */
    list_for_each_upollentry(ent, nx, &ut->poll_entries) {
    }
}
