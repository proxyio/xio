#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "channel_base.h"

/* Backend poller wait kernel timeout msec */
#define PIO_POLLER_TIMEOUT 1
#define PIO_POLLER_IOMAX 100

/* Default input/output buffer size */
static int PIO_SNDBUFSZ = 10485760;
static int PIO_RCVBUFSZ = 10485760;


struct channel_global cn_global = {};


extern int has_closed_channel(struct channel_poll *po);
extern void push_closed_channel(struct channel *cn);
extern struct channel *pop_closed_channel(struct channel_poll *po);


uint32_t msg_iovlen(char *payload) {
    struct channel_msg *msg = cont_of(payload, struct channel_msg, hdr.payload);
    return sizeof(msg->hdr) + msg->hdr.size;
}

char *msg_iovbase(char *payload) {
    struct channel_msg *msg = cont_of(payload, struct channel_msg, hdr.payload);
    return (char *)&msg->hdr;
}

char *channel_allocmsg(uint32_t size) {
    struct channel_msg *msg;
    char *chunk = (char *)mem_zalloc(sizeof(*msg) + size);
    if (!chunk)
	return NULL;
    msg = (struct channel_msg *)chunk;
    msg->hdr.size = size;
    msg->hdr.checksum = crc16((char *)&msg->hdr.size, 4);
    return msg->hdr.payload;
}

void channel_freemsg(char *payload) {
    struct channel_msg *msg = cont_of(payload, struct channel_msg, hdr.payload);
    mem_free(msg, sizeof(*msg) + msg->hdr.size);
}

uint32_t channel_msglen(char *payload) {
    struct channel_msg *msg = cont_of(payload, struct channel_msg, hdr.payload);
    return msg->hdr.size;
}


static int choose_backend_poll(int cd) {
    return cd % cn_global.npolls;
}

int alloc_cid() {
    int cd;
    mutex_lock(&cn_global.lock);
    BUG_ON(cn_global.nchannels >= PIO_MAX_CHANNELS);
    cd = cn_global.unused[cn_global.nchannels++];
    mutex_unlock(&cn_global.lock);
    return cd;
}

void free_cid(int cd) {
    mutex_lock(&cn_global.lock);
    cn_global.unused[--cn_global.nchannels] = cd;
    mutex_unlock(&cn_global.lock);
}

struct channel *cid_to_channel(int cd) {
    return &cn_global.channels[cd];
}

static void channel_base_init(int cd) {
    struct channel *cn = cid_to_channel(cd);

    mutex_init(&cn->lock);
    condition_init(&cn->cond);
    cn->fasync = false;
    cn->fok = true;
    cn->fclosed = false;
    cn->parent = -1;
    cn->cd = cd;
    cn->pollid = choose_backend_poll(cd);
    cn->ev.events = 0;
    cn->ev.f = 0;
    cn->ev.self = 0;
    cn->rcv_waiters = 0;
    cn->snd_waiters = 0;
    cn->rcv = 0;
    cn->snd = 0;
    cn->rcv_wnd = PIO_RCVBUFSZ;
    cn->snd_wnd = PIO_SNDBUFSZ;
    INIT_LIST_HEAD(&cn->rcv_head);
    INIT_LIST_HEAD(&cn->snd_head);
    INIT_LIST_HEAD(&cn->upoll_head);
    INIT_LIST_HEAD(&cn->closing_link);
}

static void channel_base_exit(int cd) {
    struct channel *cn = cid_to_channel(cd);
    struct list_head head = {};
    struct channel_msg *pos, *nx;

    mutex_destroy(&cn->lock);
    condition_destroy(&cn->cond);
    cn->ty = -1;
    cn->pf = -1;
    cn->fasync = 0;
    cn->fok = 0;
    cn->fclosed = 0;
    cn->cd = -1;
    cn->pollid = -1;
    cn->ev.events = -1;
    cn->ev.f = 0;
    cn->ev.self = 0;
    cn->rcv_waiters = -1;
    cn->snd_waiters = -1;
    cn->rcv = -1;
    cn->snd = -1;
    cn->rcv_wnd = -1;
    cn->snd_wnd = -1;

    INIT_LIST_HEAD(&head);
    list_splice(&cn->rcv_head, &head);
    list_splice(&cn->snd_head, &head);
    list_for_each_channel_msg_safe(pos, nx, &head) {
	channel_freemsg(pos->hdr.payload);
    }
    BUG_ON(attached(&cn->closing_link));

    /* It's possible that user call channel_close() and upoll_add()
     * at the same time. and attach_to_channel() happen after channel_close().
     * this is a user's bug.
     */
    BUG_ON(!list_empty(&cn->upoll_head));
}


struct channel *alloc_channel() {
    int cd = alloc_cid();
    struct channel *cn = cid_to_channel(cd);
    channel_base_init(cd);
    return cn;
}

void free_channel(struct channel *cn) {
    int cd = cn->cd;
    channel_base_exit(cd);
    free_cid(cd);
}

int alloc_pid() {
    int pd;
    mutex_lock(&cn_global.lock);
    BUG_ON(cn_global.npolls >= PIO_MAX_CPUS);
    pd = cn_global.poll_unused[cn_global.npolls++];
    mutex_unlock(&cn_global.lock);
    return pd;
}

void free_pid(int pd) {
    mutex_lock(&cn_global.lock);
    cn_global.poll_unused[--cn_global.npolls] = pd;
    mutex_unlock(&cn_global.lock);
}

struct channel_poll *pid_to_channel_poll(int pd) {
    return &cn_global.polls[pd];
}


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

static inline int event_runner(void *args) {
    int rc = 0;
    int pd = alloc_pid();
    struct channel *closing_cn;
    struct channel_poll *po = pid_to_channel_poll(pd);

    spin_init(&po->lock);
    INIT_LIST_HEAD(&po->closing_head);

    /* Init eventloop */
    BUG_ON(eloop_init(&po->el, PIO_MAX_CHANNELS/PIO_MAX_CPUS,
		      PIO_POLLER_IOMAX, PIO_POLLER_TIMEOUT) != 0);
    while (!cn_global.exiting || has_closed_channel(po)) {
	eloop_once(&po->el);
	while ((closing_cn = pop_closed_channel(po)))
	    closing_cn->vf->destroy(closing_cn->cd);
    }

    /* Release the poll descriptor when runner exit. */
    free_pid(pd);
    eloop_destroy(&po->el);
    spin_destroy(&po->lock);
    return rc;
}


extern struct channel_vf *inproc_channel_vfptr;
extern struct channel_vf *ipc_channel_vfptr;
extern struct channel_vf *tcp_channel_vfptr;


void global_channel_init() {
    int cd; /* Channel id */
    int pd; /* Poller id */
    int i;

    cn_global.exiting = false;
    mutex_init(&cn_global.lock);

    for (cd = 0; cd < PIO_MAX_CHANNELS; cd++)
	cn_global.unused[cd] = cd;
    for (pd = 0; pd < PIO_MAX_CPUS; pd++)
	cn_global.poll_unused[pd] = pd;

    cn_global.cpu_cores = 2;
    taskpool_init(&cn_global.tpool, cn_global.cpu_cores);
    taskpool_start(&cn_global.tpool);
    for (i = 0; i < cn_global.cpu_cores; i++)
	taskpool_run(&cn_global.tpool, event_runner, NULL);

    /* The priority of channel_vf: inproc > ipc > tcp */
    INIT_LIST_HEAD(global_vf_head);
    list_add_tail(&inproc_channel_vfptr->vf_item, global_vf_head);
    list_add_tail(&ipc_channel_vfptr->vf_item, global_vf_head);
    list_add_tail(&tcp_channel_vfptr->vf_item, global_vf_head);
}

void global_channel_exit() {
    cn_global.exiting = true;
    taskpool_stop(&cn_global.tpool);
    taskpool_destroy(&cn_global.tpool);
    mutex_destroy(&cn_global.lock);
}

void channel_close(int cd) {
    struct channel *cn = cid_to_channel(cd);
    struct upoll_tb *tb;
    struct upoll_entry *ent, *nx;
    
    mutex_lock(&cn->lock);
    list_for_each_channel_ent(ent, nx, &cn->upoll_head) {
	tb = cont_of(ent->notify, struct upoll_tb, notify);
	upoll_ctl(tb, UPOLL_DEL, &ent->event);
	__detach_from_channel(ent);
	entry_put(ent);
    }
    mutex_unlock(&cn->lock);
    /* Let backend thread do the last destroy() */
    push_closed_channel(cn);
}

int channel_accept(int cd) {
    struct channel *cn = cid_to_channel(cd);
    struct channel *new = alloc_channel();
    struct channel_vf *vf, *nx;

    if (!cn->fok) {
	errno = EPIPE;
	goto EXIT;
    }
    new->ty = CHANNEL_ACCEPTER;
    new->pf = cn->pf;
    new->parent = cd;
    list_for_each_channel_vf_safe(vf, nx, &cn_global.channel_vf_head) {
	new->vf = vf;
	if ((cn->pf & vf->pf) && vf->init(new->cd) == 0) {
	    return new->cd;
	}
    }
 EXIT:
    free_channel(new);
    return -1;
}

int channel_listen(int pf, const char *addr) {
    struct channel *new = alloc_channel();
    struct channel_vf *vf, *nx;

    new->ty = CHANNEL_LISTENER;
    new->pf = pf;
    ZERO(new->addr);
    strncpy(new->addr, addr, TP_SOCKADDRLEN);
    list_for_each_channel_vf_safe(vf, nx, &cn_global.channel_vf_head) {
	new->vf = vf;
	if ((pf & vf->pf) && vf->init(new->cd) == 0)
	    return new->cd;
    }
    free_channel(new);
    return -1;
}

int channel_connect(int pf, const char *peer) {
    struct channel *new = alloc_channel();
    struct channel_vf *vf, *nx;

    new->ty = CHANNEL_CONNECTOR;
    new->pf = pf;

    ZERO(new->peer);
    strncpy(new->peer, peer, TP_SOCKADDRLEN);
    list_for_each_channel_vf_safe(vf, nx, &cn_global.channel_vf_head) {
	new->vf = vf;
	if ((pf & vf->pf) && vf->init(new->cd) == 0) {
	    return new->cd;
	}
    }
    free_channel(new);
    return -1;
}

int channel_setopt(int cd, int opt, void *val, int valsz) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    if (!val || valsz <= 0) {
	errno = EINVAL;
	return -1;
    }
    switch (opt) {
    case CHANNEL_POLL:
	mutex_lock(&cn->lock);
	cn->ev = *((struct channel_events *)val);
	mutex_unlock(&cn->lock);
	break;
    case CHANNEL_SNDBUF:
	mutex_lock(&cn->lock);
	cn->snd_wnd = (*(int *)val);
	mutex_unlock(&cn->lock);
	break;
    case CHANNEL_RCVBUF:
	mutex_lock(&cn->lock);
	cn->rcv_wnd = (*(int *)val);
	mutex_unlock(&cn->lock);
	break;
    default:
	errno = EINVAL;
	return -1;
    }
    return rc;
}

int channel_getopt(int cd, int opt, void *val, int valsz) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    if (!val || valsz <= 0) {
	errno = EINVAL;
	return -1;
    }
    switch (opt) {
    case CHANNEL_POLL:
	mutex_lock(&cn->lock);
	if (cn->ev.events)
	    *((struct channel_events *)val) = cn->ev;
	else {
	    errno = ENOENT;
	    rc = -1;
	}
	mutex_unlock(&cn->lock);
	break;
    case CHANNEL_SNDBUF:
	mutex_lock(&cn->lock);
	*(int *)val = cn->snd_wnd;
	mutex_unlock(&cn->lock);
	break;
    case CHANNEL_RCVBUF:
	mutex_lock(&cn->lock);
	*(int *)val = cn->rcv_wnd;
	mutex_unlock(&cn->lock);
	break;
    default:
	errno = EINVAL;
	return -1;
    }
    return rc;
}

struct channel_msg *pop_rcv(struct channel *cn) {
    struct channel_msg *msg = NULL;
    struct channel_vf *vf = cn->vf;
    int64_t msgsz;
    uint32_t events = 0;

    mutex_lock(&cn->lock);
    while (list_empty(&cn->rcv_head) && !cn->fasync) {
	cn->rcv_waiters++;
	condition_wait(&cn->cond, &cn->lock);
	cn->rcv_waiters--;
    }
    if (!list_empty(&cn->rcv_head)) {
	msg = list_first(&cn->rcv_head, struct channel_msg, item);
	list_del_init(&msg->item);
	msgsz = msg_iovlen(msg->hdr.payload);
	cn->rcv -= msgsz;
	events |= MQ_POP;
	if (cn->rcv_wnd - cn->rcv <= msgsz)
	    events |= MQ_NONFULL;
	if (list_empty(&cn->rcv_head)) {
	    BUG_ON(cn->rcv);
	    events |= MQ_EMPTY;
	}
    }

    if (events)
	vf->rcv_notify(cn->cd, events);
    mutex_unlock(&cn->lock);
    return msg;
}

void push_rcv(struct channel *cn, struct channel_msg *msg) {
    struct channel_vf *vf = cn->vf;
    uint32_t events = 0;
    int64_t msgsz = msg_iovlen(msg->hdr.payload);

    mutex_lock(&cn->lock);
    if (list_empty(&cn->rcv_head))
	events |= MQ_NONEMPTY;
    if (cn->rcv_wnd - cn->rcv <= msgsz)
	events |= MQ_FULL;
    events |= MQ_PUSH;
    cn->rcv += msgsz;
    list_add_tail(&msg->item, &cn->rcv_head);    

    /* Wakeup the blocking waiters. */
    if (cn->rcv_waiters > 0)
	condition_broadcast(&cn->cond);

    if (events)
	vf->rcv_notify(cn->cd, events);
    mutex_unlock(&cn->lock);
}


struct channel_msg *pop_snd(struct channel *cn) {
    struct channel_vf *vf = cn->vf;
    struct channel_msg *msg = NULL;
    int64_t msgsz;
    uint32_t events = 0;
    
    mutex_lock(&cn->lock);
    if (!list_empty(&cn->snd_head)) {
	msg = list_first(&cn->snd_head, struct channel_msg, item);
	list_del_init(&msg->item);
	msgsz = msg_iovlen(msg->hdr.payload);
	cn->snd -= msgsz;
	events |= MQ_POP;
	if (cn->snd_wnd - cn->snd <= msgsz)
	    events |= MQ_NONFULL;
	if (list_empty(&cn->snd_head)) {
	    BUG_ON(cn->snd);
	    events |= MQ_EMPTY;
	}

	/* Wakeup the blocking waiters */
	if (cn->snd_waiters > 0)
	    condition_broadcast(&cn->cond);
    }

    if (events)
	vf->snd_notify(cn->cd, events);
    mutex_unlock(&cn->lock);
    return msg;
}

int push_snd(struct channel *cn, struct channel_msg *msg) {
    int rc = -1;
    struct channel_vf *vf = cn->vf;
    uint32_t events = 0;
    int64_t msgsz = msg_iovlen(msg->hdr.payload);

    mutex_lock(&cn->lock);
    while (!can_send(cn) && !cn->fasync) {
	cn->snd_waiters++;
	condition_wait(&cn->cond, &cn->lock);
	cn->snd_waiters--;
    }
    if (can_send(cn)) {
	rc = 0;
	if (list_empty(&cn->snd_head))
	    events |= MQ_NONEMPTY;
	if (cn->snd_wnd - cn->snd <= msgsz)
	    events |= MQ_FULL;
	events |= MQ_PUSH;
	cn->snd += msgsz;
	list_add_tail(&msg->item, &cn->snd_head);
    }

    if (events)
	vf->snd_notify(cn->cd, events);
    mutex_unlock(&cn->lock);
    return rc;
}

int channel_recv(int cd, char **payload) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);
    struct channel_msg *msg;

    if (!payload) {
	errno = EINVAL;
	return -1;
    }
    if (!(msg = pop_rcv(cn))) {
	errno = cn->fok ? EAGAIN : EPIPE;
	rc = -1;
    } else
	*payload = msg->hdr.payload;
    return rc;
}

int channel_send(int cd, char *payload) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);
    struct channel_msg *msg;
    
    if (!payload) {
	errno = EINVAL;
	return -1;
    }
    msg = cont_of(payload, struct channel_msg, hdr.payload);
    if ((rc = push_snd(cn, msg)) < 0) {
	errno = cn->fok ? EAGAIN : EPIPE;
    }
    return rc;
}

void update_upoll_tb(struct channel *cn) {
    int events = 0;
    struct upoll_entry *ent, *nx;

    mutex_lock(&cn->lock);
    events |= !list_empty(&cn->rcv_head) ? UPOLLIN : 0;
    events |= can_send(cn) ? UPOLLOUT : 0;
    events |= !cn->fok ? UPOLLERR : 0;
    list_for_each_channel_ent(ent, nx, &cn->upoll_head) {
	if (events) {
	    ent->event.happened = events;
	    ent->notify->event(ent->notify, ent);
	}
    }
    mutex_unlock(&cn->lock);
}
