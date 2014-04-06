#include "os/timesz.h"
#include "base.h"
#include "channel_base.h"

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
    list_add_tail(&cn->closing_link, &po->closing_head);
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
static void upoll_event(struct upoll_notify *un, struct upoll_entry *ent) {
    struct upoll_table *ut = cont_of(un, struct upoll_table, notify);

    /* If any happened, insert it into happened_entries list */
    mutex_lock(&ut->lock);
    if (ent->cn.events_happened && !attached(&ent->happened_link)) {
	list_add_tail(&ent->happened_link, &ut->happened_entries);
	if (ut->uwaiters)
	    condition_broadcast(&ut->cond);
    }
    mutex_unlock(&ut->lock);
}


struct upoll_table *upoll_create() {
    struct upoll_table *ut = (struct upoll_table *)mem_zalloc(sizeof(*ut));
    if (ut) {
	ut->notify.event = upoll_event;
	ut->uwaiters = 0;
	mutex_init(&ut->lock);
	ut->cur_happened_events = 0;
	INIT_LIST_HEAD(&ut->happened_entries);
	INIT_LIST_HEAD(&ut->poll_entries);
    }
    return ut;
}

static void upoll_add_entry(struct upoll_table *ut, struct upoll_entry *ent) {
    mutex_lock(&ut->lock);
    ut->ent_size++;
    list_add_tail(&ent->polltable_link, &ut->poll_entries);
    mutex_unlock(&ut->lock);
}

static void upoll_rm_entry(struct upoll_table *ut, struct upoll_entry *ent) {
    mutex_lock(&ut->lock);
    ut->ent_size--;
    list_del_init(&ent->polltable_link);
    mutex_unlock(&ut->lock);
}

static int upoll_add(struct upoll_table *ut, struct pollcn *cn) {
    int rc = -1;
    struct upoll_entry *ent = (struct upoll_entry *)mem_zalloc(sizeof(*ent));    
    if (ent) {
	INIT_LIST_HEAD(&ent->channel_link);
	INIT_LIST_HEAD(&ent->polltable_link);
	INIT_LIST_HEAD(&ent->happened_link);
	ent->cn = *cn;
	ent->notify = &ut->notify;
	upoll_add_entry(ut, ent);
	channel_add_upoll_entry(ent);
	return rc;
    }
    return rc;
}

static struct upoll_entry *upoll_find(struct upoll_table *ut, int cd) {
    struct upoll_entry *ent = NULL, *pos, *nx;

    mutex_lock(&ut->lock);
    /* TODO: we can use a rbtree make better performance */
    list_for_each_upollentry(pos, nx, &ut->poll_entries) {
	if (pos->cn.cd == cd) {
	    ent = pos;
	    break;
	}
    }
    mutex_unlock(&ut->lock);
    return ent;
}

static int upoll_rm(struct upoll_table *ut, struct upoll_entry *ent) {
    int rc = 0;
    channel_rm_upoll_entry(ent);
    upoll_rm_entry(ut, ent);
    return rc;
}

int upoll_ctl(struct upoll_table *ut, struct pollcn *cn) {
    int rc = -1;
    struct upoll_entry *ent = upoll_find(ut, cn->cd);

    if (ent && !cn->events)
	/* Remove */
	return upoll_rm(ut, ent);
    else if (ent && cn->events != ent->cn.events) {
	/* Mod */
	mutex_lock(&ut->lock);
	ent->cn.events = cn->events;
	mutex_unlock(&ut->lock);
    } else if (!ent)
	/* Add a new poll entry */
	return upoll_add(ut, cn);
    return rc;
}

int upoll_wait(struct upoll_table *ut, struct pollcn *cns, int ncn, int timeout) {
    int n = 0;
    int64_t end = rt_mstime() + timeout;
    struct pollcn *cn = cns;
    struct upoll_entry *ent, *nx;
    
    mutex_lock(&ut->lock);
    while (ut->cur_happened_events < 0 && rt_mstime() < end) {
	ut->uwaiters++;
	condition_wait(&ut->cond, &ut->lock);
	ut->uwaiters--;
    }
    if (ut->cur_happened_events > 0) {
	if (ncn > ut->cur_happened_events)
	    ncn = ut->cur_happened_events;
	n = ncn;
	list_for_each_upollentry(ent, nx, &ut->happened_entries) {
	    if (ncn <= 0)
		break;
	    assert(ent->cn.events_happened);
	    list_del_init(&ent->happened_link);

	    /* Copy events detail into user-space buf and then clear */
	    *cns = ent->cn;
	    ent->cn.events_happened = 0;
	    ncn--;
	    cns++;
	}
	assert(cns - cn == n);
    }
    mutex_unlock(&ut->lock);
    return n;
}

void upoll_close(struct upoll_table *ut) {
    struct upoll_entry *ent, *nx;

    /* TODO: BUGON here */
    list_for_each_upollentry(ent, nx, &ut->poll_entries) {
	upoll_rm(ut, ent);
    }
}
