#include <os/timesz.h>
#include <base.h>
#include "xpoll_struct.h"

const char *xpoll_str[] = {
    "",
    "XPOLLIN",
    "XPOLLOUT",
    "XPOLLIN|XPOLLOUT",
    "XPOLLERR",
    "XPOLLIN|XPOLLERR",
    "XPOLLOUT|XPOLLERR",
    "XPOLLIN|XPOLLOUT|XPOLLERR",
};


static void
event_notify(struct xpoll_notify *un, struct xpoll_entry *ent, u32 ev);

struct xpoll_t *xpoll_create() {
    struct xpoll_t *po = xpoll_new();

    if (po) {
	po->notify.event = event_notify;
	xpoll_get(po);
    }
    return po;
}

static void
event_notify(struct xpoll_notify *un, struct xpoll_entry *ent, u32 ev) {
    struct xpoll_t *po = cont_of(un, struct xpoll_t, notify);

    mutex_lock(&po->lock);
    BUG_ON(!ent->event.care);
    if (!attached(&ent->lru_link)) {
	__detach_from_xsock(ent);
	mutex_unlock(&po->lock);
	xent_put(ent);
	return;
    }
    spin_lock(&ent->lock);
    if (ev) {
	DEBUG_OFF("xsock %d update events %s", ent->event.xd, xpoll_str[ev]);
	ent->event.happened = ev;
	list_move(&ent->lru_link, &po->lru_head);
	if (po->uwaiters)
	    condition_broadcast(&po->cond);
    } else if (ent->event.happened && !ev) {
	DEBUG_OFF("xsock %d disable events notify", ent->event.xd);
	ent->event.happened = 0;
	list_move_tail(&ent->lru_link, &po->lru_head);
    }
    spin_unlock(&ent->lock);
    mutex_unlock(&po->lock);
}

extern void xpoll_notify(struct xsock *sx, u32 vf_spec);

static int xpoll_add(struct xpoll_t *po, struct xpoll_event *event) {
    struct xpoll_entry *ent = xpoll_getent(po, event->xd);
    struct xsock *sx = xget(event->xd);

    if (!ent)
	return -1;

    /* Set up events callback */
    spin_lock(&ent->lock);
    ent->event.xd = event->xd;
    ent->event.care = event->care;
    ent->event.self = event->self;
    ent->notify = &po->notify;
    spin_unlock(&ent->lock);

    /* We hold a ref here. it is used for xsock */
    /* BUG case 1: it's possible that this entry was deleted by xpoll_rm() */
    attach_to_xsock(ent, sx->xd);

    xpoll_notify(sx, 0);
    return 0;
}

static int xpoll_rm(struct xpoll_t *po, struct xpoll_event *event) {
    struct xpoll_entry *ent = xpoll_putent(po, event->xd);

    if (!ent) {
	return -1;
    }

    /* Release the ref hold by xpoll_t */
    xent_put(ent);
    return 0;
}


static int xpoll_mod(struct xpoll_t *po, struct xpoll_event *event) {
    struct xsock *sx = xget(event->xd);
    struct xpoll_entry *ent = xpoll_find(po, event->xd);

    if (!ent)
	return -1;
    mutex_lock(&po->lock);
    spin_lock(&ent->lock);
    ent->event.care = event->care;
    spin_unlock(&ent->lock);
    mutex_unlock(&po->lock);

    xpoll_notify(sx, 0);

    /* Release the ref hold by caller */
    xent_put(ent);
    return 0;
}

int xpoll_ctl(struct xpoll_t *po, int op, struct xpoll_event *event) {
    int rc = -1;

    switch (op) {
    case XPOLL_ADD:
	rc = xpoll_add(po, event);
	return rc;
    case XPOLL_DEL:
	rc = xpoll_rm(po, event);
	return rc;
    case XPOLL_MOD:
	rc = xpoll_mod(po, event);
	return rc;
    default:
	errno = EINVAL;
    }
    return -1;
}

int xpoll_wait(struct xpoll_t *po, struct xpoll_event *ev_buf,
	       int size, u32 timeout) {
    int n = 0;
    struct xpoll_entry *ent, *nx;

    mutex_lock(&po->lock);

    /* If havn't any events here. we wait */
    ent = list_first(&po->lru_head, struct xpoll_entry, lru_link);
    if (!ent->event.happened && timeout > 0) {
	po->uwaiters++;
	condition_timedwait(&po->cond, &po->lock, timeout);
	po->uwaiters--;
    }
    xpoll_walk_ent(ent, nx, &po->lru_head) {
	if (!ent->event.happened || n >= size)
	    break;
	ev_buf[n++] = ent->event;
    }
    mutex_unlock(&po->lock);
    return n;
}


void xpoll_close(struct xpoll_t *po) {
    struct xpoll_entry *ent;

    while ((ent = xpoll_popent(po))) {
	/* Release the ref hold by xpoll_t */
	xent_put(ent);
    }
    /* Release the ref hold by user-caller */
    xpoll_put(po);
}
