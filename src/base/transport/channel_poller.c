#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "channel_poller.h"
#include "sync/mutex.h"
#include "channel.h"

extern struct channel_global cn_global;

struct channelpoller_global {
    mutex_t lock;

    /*  Stack of unused file descriptors. */
    int unused[PIO_MAX_CHANNELPOLLERS];

    /*  Number of actual open channelpollers in the table. */
    size_t npollers;


    /*  The global table of existing poller. The descriptor representing
        the poller is the index to this table. This pointer is also used to
        find out whether context is initialised. If it is NULL, context is
        uninitialised. */
    epoll_t els[PIO_MAX_CHANNELPOLLERS];
    
    /* channelpoller corresponse ready or empty channel head */
    spin_t slot_lock[PIO_MAX_CHANNELPOLLERS];
    struct list_head ready_head[PIO_MAX_CHANNELPOLLERS];
    struct list_head empty_head[PIO_MAX_CHANNELPOLLERS];
};

#define list_for_each_channel_safe(pos, nx, head)			\
    list_for_each_entry_safe(pos, nx, head, struct channel, poller_item)

static struct channelpoller_global cp_global = {};


void channelpoller_global_init() {
    int pd;

    mutex_init(&cp_global.lock);
    for (pd = 0; pd < PIO_MAX_CHANNELPOLLERS; pd++) {
	cp_global.unused[pd] = pd;
	spin_init(&cp_global.slot_lock[pd]);
	INIT_LIST_HEAD(&cp_global.ready_head[pd]);
	INIT_LIST_HEAD(&cp_global.empty_head[pd]);
    }
}


int channelpoller_create() {
    int pd;
    epoll_t *el;

    mutex_lock(&cp_global.lock);
    pd = cp_global.unused[cp_global.npollers++];
    mutex_unlock(&cp_global.lock);

    el = &cp_global.els[pd];
    if (epoll_init(el, 1024, 1024, 1) < 0) {
	mutex_lock(&cp_global.lock);
	cp_global.unused[--cp_global.npollers] = pd;
	mutex_unlock(&cp_global.lock);
	return -1;
    }
    return pd;
}

int channelpoller_close(int pd) {
    epoll_t *el = &cp_global.els[pd];

    epoll_destroy(el);
    mutex_lock(&cp_global.lock);
    cp_global.unused[--cp_global.npollers] = pd;
    mutex_unlock(&cp_global.lock);
    return 0;
}


static int msg_ready(struct bio *b, int64_t *payload_sz, int64_t *control_sz) {
    struct channel_msg_item msgi = {};

    if (b->bsize < sizeof(msgi.hdr))
	return false;
    bio_copy(b, (char *)&msgi.hdr, sizeof(msgi.hdr));
    if (b->bsize < channel_msgiov_len(&msgi->msg))
	return false;
    *payload_sz = msgi.hdr.payload_sz;
    *control_sz = msgi.hdr.control_sz;
    return true;
}


static int channel_event_handler(epoll_t *el, epollevent_t *et) {
    int rc = 0;
    int pd = el - &cp_global.els[0];
    struct channel *cn = pio_cont(et, struct channel, et);
    struct channel_msg *msg;
    struct channel_msg_item *msgi;
    int64_t payload_sz = 0, control_sz = 0;

    assert(&cp_global.els[1] - &cp_global.els[0] == 1);
    spin_lock(&cn->lock);
    if (et->events & EPOLLIN) {
	rc = bio_prefetch(&cn->in, &cn->sock_ops);
	while (msg_ready(&cn->in, &payload_sz, &control_sz)) {
	    if (!attached(&cn->poller_item))
		list_add_tail(&cn->poller_item, &cp_global.ready_head[pd]);
	    msg = channel_allocmsg(payload_sz, control_sz);
	    bio_read(&cn->in, channel_msgiov_base(msg), channel_msgiov_len(msg));
	    msgi = pio_cont(msg, struct channel_msg_item, item);
	    list_add_tail(&msgi->item, &cn->rcv_head);
	}
	if (rc < 0 && errno != EAGAIN) {
	    spin_unlock(&cn->lock);
	    return -1;
	}
    }
    if (et->events & EPOLLOUT) {
	while (!list_empty(&cn->snd_head)) {
	    msgi = list_first(&cn->snd_head, struct channel_msg_item, item);
	    list_del_init(&msgi->item);
	    msg = &msgi->msg;
	    bio_write(&cn->out, channel_msgiov_base(msg), channel_msgiov_len(msg));
	}
	if ((rc = bio_flush(&cn->out, &cn->sock_ops)) < 0 && errno != EAGAIN) {
	    spin_unlock(&cn->lock);
	    return -1;
	}
    }
    spin_unlock(&cn->lock);
    return 0;
}


int channelpoller_setev(int pd, int cd, struct channel_event *ev) {
    int rc = -EINVAL;
    uint32_t epoll_events = 0;
    spin_t *lock = &cp_global.slot_lock[pd];
    epoll_t *el = &cp_global.els[pd];
    struct channel *cn = &cn_global.channels[cd];
    
    epoll_events |= (ev->events & CHANNEL_MSGIN) ? EPOLLIN : 0;
    epoll_events |= (ev->events & CHANNEL_MSGOUT) ? EPOLLOUT : 0;
    epoll_events |= EPOLLRDHUP|EPOLLERR;

    spin_lock(lock);
    spin_lock(&cn->lock);
    cn->et.f = channel_event_handler;
    cn->et.data = ev->ptr;
    cn->et.fd = cn->fd;
    if (!cn->et.events && epoll_events) {
	cn->pd = pd;
	rc = epoll_add(el, &cn->et);
    } else if (cn->et.events && !epoll_events) {
	cn->pd = -1;
	rc = epoll_del(el, &cn->et);
	if (attached(&cn->poller_item))
	    list_del_init(&cn->poller_item);
    } else if (cn->et.events != epoll_events) {
	rc = epoll_mod(el, &cn->et);
    }
    spin_unlock(&cn->lock);
    spin_unlock(lock);
    return rc;
}


int channelpoller_getev(int pd, int cd, struct channel_event *ev) {
    int rc = 0;
    uint32_t epoll_events = 0;
    struct channel *cn = &cn_global.channels[cd];

    spin_lock(&cn->lock);
    epoll_events = cn->et.events;
    ev->events |= (epoll_events & EPOLLIN) ? CHANNEL_MSGIN : 0;
    ev->events |= (epoll_events & EPOLLOUT) ? CHANNEL_MSGOUT : 0;
    ev->ptr = cn->et.data;
    spin_unlock(&cn->lock);
    return rc;
}


int channelpoller_wait(int pd, struct channel_event *ready_channels,
        int max_events) {
    spin_t *lock = &cp_global.slot_lock[pd];
    epoll_t *el = &cp_global.els[pd];
    struct channel *cn, *nx;
    struct channel_event *eptr = ready_channels;
    
    spin_lock(lock);
    epoll_oneloop(el);
    list_for_each_channel_safe(cn, nx, &cp_global.ready_head[pd]) {
	if (eptr >= ready_channels + max_events)
	    break;
	spin_lock(&cn->lock);
	if (list_empty(&cn->rcv_head))
	    list_del_init(&cn->poller_item);
	else
	    eptr->events = CHANNEL_MSGIN;
	eptr->events |= CHANNEL_MSGOUT;
	eptr->events |= (cn->et.happened & EPOLLERR) ? CHANNEL_ERROR : 0;
	spin_unlock(&cn->lock);
	eptr++;
    }
    spin_unlock(lock);
    return eptr - ready_channels;
}

