#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sync/waitgroup.h>
#include "runner/taskpool.h"
#include "xbase.h"

/* Backend poller wait kernel timeout msec */
#define PIO_POLLER_TIMEOUT 1
#define PIO_POLLER_IOMAX 100

/* Default input/output buffer size */
static int PIO_SNDBUFSZ = 10485760;
static int PIO_RCVBUFSZ = 10485760;


struct xglobal xglobal = {};


extern int has_closed_channel(struct xtaskor *po);
extern void push_closed_channel(struct xsock *xs);
extern struct xsock *pop_closed_channel(struct xtaskor *po);

void __po_notify(struct xsock *xs, u32 vf_spec);
void xpoll_notify(struct xsock *xs, u32 vf_spec);

uint32_t msg_iovlen(char *payload) {
    struct xmsg *msg = cont_of(payload, struct xmsg, hdr.payload);
    return sizeof(msg->hdr) + msg->hdr.size;
}

char *msg_iovbase(char *payload) {
    struct xmsg *msg = cont_of(payload, struct xmsg, hdr.payload);
    return (char *)&msg->hdr;
}

char *xallocmsg(uint32_t size) {
    struct xmsg *msg;
    char *chunk = (char *)mem_zalloc(sizeof(*msg) + size);
    if (!chunk)
	return NULL;
    msg = (struct xmsg *)chunk;
    msg->hdr.size = size;
    msg->hdr.checksum = crc16((char *)&msg->hdr.size, 4);
    return msg->hdr.payload;
}

void xfreemsg(char *payload) {
    struct xmsg *msg = cont_of(payload, struct xmsg, hdr.payload);
    mem_free(msg, sizeof(*msg) + msg->hdr.size);
}

uint32_t xmsglen(char *payload) {
    struct xmsg *msg = cont_of(payload, struct xmsg, hdr.payload);
    return msg->hdr.size;
}


static int choose_backend_poll(int cd) {
    return cd % xglobal.npolls;
}

int xd_alloc() {
    int cd;
    mutex_lock(&xglobal.lock);
    BUG_ON(xglobal.nsocks >= PIO_MAX_SOCKS);
    cd = xglobal.unused[xglobal.nsocks++];
    mutex_unlock(&xglobal.lock);
    return cd;
}

void xd_free(int cd) {
    mutex_lock(&xglobal.lock);
    xglobal.unused[--xglobal.nsocks] = cd;
    mutex_unlock(&xglobal.lock);
}

struct xsock *cid_to_channel(int cd) {
    return &xglobal.socks[cd];
}

static void xbase_init(int cd) {
    struct xsock *xs = cid_to_channel(cd);

    mutex_init(&xs->lock);
    condition_init(&xs->cond);
    xs->fasync = false;
    xs->fok = true;
    xs->fclosed = false;
    xs->parent = -1;
    xs->cd = cd;
    xs->pollid = choose_backend_poll(cd);
    xs->rcv_waiters = 0;
    xs->snd_waiters = 0;
    xs->rcv = 0;
    xs->snd = 0;
    xs->rcv_wnd = PIO_RCVBUFSZ;
    xs->snd_wnd = PIO_SNDBUFSZ;
    INIT_LIST_HEAD(&xs->rcv_head);
    INIT_LIST_HEAD(&xs->snd_head);
    INIT_LIST_HEAD(&xs->xpoll_head);
    INIT_LIST_HEAD(&xs->closing_link);
}

static void xbase_exit(int cd) {
    struct xsock *xs = cid_to_channel(cd);
    struct list_head head = {};
    struct xmsg *pos, *nx;

    mutex_destroy(&xs->lock);
    condition_destroy(&xs->cond);
    xs->ty = -1;
    xs->pf = -1;
    xs->fasync = 0;
    xs->fok = 0;
    xs->fclosed = 0;
    xs->cd = -1;
    xs->pollid = -1;
    xs->rcv_waiters = -1;
    xs->snd_waiters = -1;
    xs->rcv = -1;
    xs->snd = -1;
    xs->rcv_wnd = -1;
    xs->snd_wnd = -1;

    INIT_LIST_HEAD(&head);
    list_splice(&xs->rcv_head, &head);
    list_splice(&xs->snd_head, &head);
    list_for_each_xmsg_safe(pos, nx, &head) {
	xfreemsg(pos->hdr.payload);
    }
    BUG_ON(attached(&xs->closing_link));

    /* It's possible that user call xclose() and xpoll_add()
     * at the same time. and attach_to_channel() happen after xclose().
     * this is a user's bug.
     */
    BUG_ON(!list_empty(&xs->xpoll_head));
}


struct xsock *xsock_alloc() {
    int cd = xd_alloc();
    struct xsock *xs = cid_to_channel(cd);
    xbase_init(cd);
    return xs;
}

void xsock_free(struct xsock *xs) {
    int cd = xs->cd;
    xbase_exit(cd);
    xd_free(cd);
}

int alloc_pid() {
    int pd;
    mutex_lock(&xglobal.lock);
    BUG_ON(xglobal.npolls >= PIO_MAX_CPUS);
    pd = xglobal.poll_unused[xglobal.npolls++];
    mutex_unlock(&xglobal.lock);
    return pd;
}

void free_pid(int pd) {
    mutex_lock(&xglobal.lock);
    xglobal.poll_unused[--xglobal.npolls] = pd;
    mutex_unlock(&xglobal.lock);
}

struct xtaskor *pid_to_xtaskor(int pd) {
    return &xglobal.polls[pd];
}


int has_closed_channel(struct xtaskor *po) {
    int has = true;
    spin_lock(&po->lock);
    if (list_empty(&po->closing_head))
	has = false;
    spin_unlock(&po->lock);
    return has;
}

void push_closed_channel(struct xsock *xs) {
    struct xtaskor *po = pid_to_xtaskor(xs->pollid);
    
    spin_lock(&po->lock);
    if (!xs->fclosed && !attached(&xs->closing_link)) {
	xs->fclosed = true;
	list_add_tail(&xs->closing_link, &po->closing_head);
    }
    spin_unlock(&po->lock);
}

struct xsock *pop_closed_channel(struct xtaskor *po) {
    struct xsock *xs = NULL;

    spin_lock(&po->lock);
    if (!list_empty(&po->closing_head)) {
	xs = list_first(&po->closing_head, struct xsock, closing_link);
	list_del_init(&xs->closing_link);
    }
    spin_unlock(&po->lock);
    return xs;
}

static inline int event_runner(void *args) {
    waitgroup_t *wg = (waitgroup_t *)args;
    int rc = 0;
    int pd = alloc_pid();
    struct xsock *closing_xs;
    struct xtaskor *po = pid_to_xtaskor(pd);

    spin_init(&po->lock);
    INIT_LIST_HEAD(&po->closing_head);

    /* Init eventloop and wakeup parent */
    BUG_ON(eloop_init(&po->el, PIO_MAX_SOCKS/PIO_MAX_CPUS,
		      PIO_POLLER_IOMAX, PIO_POLLER_TIMEOUT) != 0);
    waitgroup_done(wg);

    while (!xglobal.exiting || has_closed_channel(po)) {
	eloop_once(&po->el);
	while ((closing_xs = pop_closed_channel(po)))
	    closing_xs->vf->destroy(closing_xs->cd);
    }

    /* Release the poll descriptor when runner exit. */
    free_pid(pd);
    eloop_destroy(&po->el);
    spin_destroy(&po->lock);
    return rc;
}


extern struct xsock_vf *inproc_xsock_vfptr;
extern struct xsock_vf *ipc_xsock_vfptr;
extern struct xsock_vf *tcp_xsock_vfptr;


void global_xinit() {
    waitgroup_t wg;
    int cd; /* Channel id */
    int pd; /* Poller id */
    int i;

    waitgroup_init(&wg);
    xglobal.exiting = false;
    mutex_init(&xglobal.lock);

    for (cd = 0; cd < PIO_MAX_SOCKS; cd++)
	xglobal.unused[cd] = cd;
    for (pd = 0; pd < PIO_MAX_CPUS; pd++)
	xglobal.poll_unused[pd] = pd;

    xglobal.cpu_cores = 2;
    taskpool_init(&xglobal.tpool, xglobal.cpu_cores);
    taskpool_start(&xglobal.tpool);
    waitgroup_adds(&wg, xglobal.cpu_cores);
    for (i = 0; i < xglobal.cpu_cores; i++)
	taskpool_run(&xglobal.tpool, event_runner, &wg);
    /* Waiting all poll's event_runner start properly */
    waitgroup_wait(&wg);
    waitgroup_destroy(&wg);
    
    /* The priority of xsock_vf: inproc > ipc > tcp */
    INIT_LIST_HEAD(global_vf_head);
    list_add_tail(&inproc_xsock_vfptr->vf_item, global_vf_head);
    list_add_tail(&ipc_xsock_vfptr->vf_item, global_vf_head);
    list_add_tail(&tcp_xsock_vfptr->vf_item, global_vf_head);
}

void global_xexit() {
    xglobal.exiting = true;
    taskpool_stop(&xglobal.tpool);
    taskpool_destroy(&xglobal.tpool);
    mutex_destroy(&xglobal.lock);
}

void xclose(int cd) {
    struct xsock *xs = cid_to_channel(cd);
    struct xpoll_t *po;
    struct xpoll_entry *ent, *nx;
    
    mutex_lock(&xs->lock);
    list_for_each_xent(ent, nx, &xs->xpoll_head) {
	po = cont_of(ent->notify, struct xpoll_t, notify);
	xpoll_ctl(po, XPOLL_DEL, &ent->event);
	__detach_from_channel(ent);
	entry_put(ent);
    }
    mutex_unlock(&xs->lock);
    /* Let backend thread do the last destroy() */
    push_closed_channel(xs);
}

int xaccept(int cd) {
    struct xsock *xs = cid_to_channel(cd);
    struct xsock *new = xsock_alloc();
    struct xsock_vf *vf, *nx;

    if (!xs->fok) {
	errno = EPIPE;
	goto EXIT;
    }
    new->ty = XACCEPTER;
    new->pf = xs->pf;
    new->parent = cd;
    xpoll_notify(xs, 0);
    list_for_each_xsock_vf_safe(vf, nx, &xglobal.xsock_vf_head) {
	new->vf = vf;
	if ((xs->pf & vf->pf) && vf->init(new->cd) == 0) {
	    return new->cd;
	}
    }
 EXIT:
    DEBUG_OFF("%d failed with errno %d", cd, errno);
    xsock_free(new);
    return -1;
}

int xlisten(int pf, const char *addr) {
    struct xsock *new = xsock_alloc();
    struct xsock_vf *vf, *nx;

    new->ty = XLISTENER;
    new->pf = pf;
    ZERO(new->addr);
    strncpy(new->addr, addr, TP_SOCKADDRLEN);
    list_for_each_xsock_vf_safe(vf, nx, &xglobal.xsock_vf_head) {
	new->vf = vf;
	if ((pf & vf->pf) && vf->init(new->cd) == 0)
	    return new->cd;
    }
    xsock_free(new);
    return -1;
}

int xconnect(int pf, const char *peer) {
    struct xsock *new = xsock_alloc();
    struct xsock_vf *vf, *nx;

    new->ty = XCONNECTOR;
    new->pf = pf;

    ZERO(new->peer);
    strncpy(new->peer, peer, TP_SOCKADDRLEN);
    list_for_each_xsock_vf_safe(vf, nx, &xglobal.xsock_vf_head) {
	new->vf = vf;
	if ((pf & vf->pf) && vf->init(new->cd) == 0) {
	    return new->cd;
	}
    }
    xsock_free(new);
    return -1;
}

int xsetopt(int cd, int opt, void *on, int size) {
    int rc = 0;
    struct xsock *xs = cid_to_channel(cd);

    if (!on || size <= 0) {
	errno = EINVAL;
	return -1;
    }
    switch (opt) {
    case XNOBLOCK:
	mutex_lock(&xs->lock);
	xs->fasync = *(int *)on ? true : false;
	mutex_unlock(&xs->lock);
	break;
    case XSNDBUF:
	mutex_lock(&xs->lock);
	xs->snd_wnd = (*(int *)on);
	mutex_unlock(&xs->lock);
	break;
    case XRCVBUF:
	mutex_lock(&xs->lock);
	xs->rcv_wnd = (*(int *)on);
	mutex_unlock(&xs->lock);
	break;
    default:
	errno = EINVAL;
	return -1;
    }
    return rc;
}

int xgetopt(int cd, int opt, void *on, int size) {
    int rc = 0;
    struct xsock *xs = cid_to_channel(cd);

    if (!on || size <= 0) {
	errno = EINVAL;
	return -1;
    }
    switch (opt) {
    case XNOBLOCK:
	mutex_lock(&xs->lock);
	*(int *)on = xs->fasync ? true : false;
	mutex_unlock(&xs->lock);
	break;
    case XSNDBUF:
	mutex_lock(&xs->lock);
	*(int *)on = xs->snd_wnd;
	mutex_unlock(&xs->lock);
	break;
    case XRCVBUF:
	mutex_lock(&xs->lock);
	*(int *)on = xs->rcv_wnd;
	mutex_unlock(&xs->lock);
	break;
    default:
	errno = EINVAL;
	return -1;
    }
    return rc;
}


struct xmsg *pop_rcv(struct xsock *xs) {
    struct xmsg *msg = NULL;
    struct xsock_vf *vf = xs->vf;
    int64_t msgsz;
    uint32_t events = 0;

    mutex_lock(&xs->lock);
    while (list_empty(&xs->rcv_head) && !xs->fasync) {
	xs->rcv_waiters++;
	condition_wait(&xs->cond, &xs->lock);
	xs->rcv_waiters--;
    }
    if (!list_empty(&xs->rcv_head)) {
	DEBUG_OFF("channel %d", xs->cd);
	msg = list_first(&xs->rcv_head, struct xmsg, item);
	list_del_init(&msg->item);
	msgsz = msg_iovlen(msg->hdr.payload);
	xs->rcv -= msgsz;
	events |= MQ_POP;
	if (xs->rcv_wnd - xs->rcv <= msgsz)
	    events |= MQ_NONFULL;
	if (list_empty(&xs->rcv_head)) {
	    BUG_ON(xs->rcv);
	    events |= MQ_EMPTY;
	}
    }

    if (events)
	vf->rcv_notify(xs->cd, events);

    mutex_unlock(&xs->lock);
    return msg;
}

void push_rcv(struct xsock *xs, struct xmsg *msg) {
    struct xsock_vf *vf = xs->vf;
    uint32_t events = 0;
    int64_t msgsz = msg_iovlen(msg->hdr.payload);

    mutex_lock(&xs->lock);
    if (list_empty(&xs->rcv_head))
	events |= MQ_NONEMPTY;
    if (xs->rcv_wnd - xs->rcv <= msgsz)
	events |= MQ_FULL;
    events |= MQ_PUSH;
    xs->rcv += msgsz;
    list_add_tail(&msg->item, &xs->rcv_head);    
    __po_notify(xs, 0);
    DEBUG_OFF("channel %d", xs->cd);

    /* Wakeup the blocking waiters. */
    if (xs->rcv_waiters > 0)
	condition_broadcast(&xs->cond);

    if (events)
	vf->rcv_notify(xs->cd, events);
    mutex_unlock(&xs->lock);
}


struct xmsg *pop_snd(struct xsock *xs) {
    struct xsock_vf *vf = xs->vf;
    struct xmsg *msg = NULL;
    int64_t msgsz;
    uint32_t events = 0;
    
    mutex_lock(&xs->lock);
    if (!list_empty(&xs->snd_head)) {
	DEBUG_OFF("channel %d", xs->cd);
	msg = list_first(&xs->snd_head, struct xmsg, item);
	list_del_init(&msg->item);
	msgsz = msg_iovlen(msg->hdr.payload);
	xs->snd -= msgsz;
	events |= MQ_POP;
	if (xs->snd_wnd - xs->snd <= msgsz)
	    events |= MQ_NONFULL;
	if (list_empty(&xs->snd_head)) {
	    BUG_ON(xs->snd);
	    events |= MQ_EMPTY;
	}

	/* Wakeup the blocking waiters */
	if (xs->snd_waiters > 0)
	    condition_broadcast(&xs->cond);
    }

    if (events)
	vf->snd_notify(xs->cd, events);

    __po_notify(xs, 0);
    mutex_unlock(&xs->lock);
    return msg;
}

int push_snd(struct xsock *xs, struct xmsg *msg) {
    int rc = -1;
    struct xsock_vf *vf = xs->vf;
    uint32_t events = 0;
    int64_t msgsz = msg_iovlen(msg->hdr.payload);

    mutex_lock(&xs->lock);
    while (!can_send(xs) && !xs->fasync) {
	xs->snd_waiters++;
	condition_wait(&xs->cond, &xs->lock);
	xs->snd_waiters--;
    }
    if (can_send(xs)) {
	rc = 0;
	if (list_empty(&xs->snd_head))
	    events |= MQ_NONEMPTY;
	if (xs->snd_wnd - xs->snd <= msgsz)
	    events |= MQ_FULL;
	events |= MQ_PUSH;
	xs->snd += msgsz;
	list_add_tail(&msg->item, &xs->snd_head);
	DEBUG_OFF("channel %d", xs->cd);
    }

    if (events)
	vf->snd_notify(xs->cd, events);

    mutex_unlock(&xs->lock);
    return rc;
}

int xrecv(int cd, char **payload) {
    int rc = 0;
    struct xsock *xs = cid_to_channel(cd);
    struct xmsg *msg;

    if (!payload) {
	errno = EINVAL;
	return -1;
    }
    if (!(msg = pop_rcv(xs))) {
	errno = xs->fok ? EAGAIN : EPIPE;
	rc = -1;
    } else
	*payload = msg->hdr.payload;
    return rc;
}

int xsend(int cd, char *payload) {
    int rc = 0;
    struct xsock *xs = cid_to_channel(cd);
    struct xmsg *msg;
    
    if (!payload) {
	errno = EINVAL;
	return -1;
    }
    msg = cont_of(payload, struct xmsg, hdr.payload);
    if ((rc = push_snd(xs, msg)) < 0) {
	errno = xs->fok ? EAGAIN : EPIPE;
    }
    return rc;
}


/* Generic xpoll_t notify function. always called by xsock_vf
 * when has any message come or can send any massage into network
 * or has a new connection wait for established.
 * here we only check the mq events and vf_spec saved the other
 * events gived by xsock_vf
 */
void __po_notify(struct xsock *xs, u32 vf_spec) {
    int events = 0;
    struct xpoll_entry *ent, *nx;

    events |= vf_spec;
    events |= !list_empty(&xs->rcv_head) ? XPOLLIN : 0;
    events |= can_send(xs) ? XPOLLOUT : 0;
    events |= !xs->fok ? XPOLLERR : 0;
    DEBUG_OFF("%d channel xpoll events %d happen", xs->cd, events);
    list_for_each_xent(ent, nx, &xs->xpoll_head) {
	ent->notify->event(ent->notify, ent, ent->event.care & events);
    }
}

void xpoll_notify(struct xsock *xs, u32 vf_spec) {
    mutex_lock(&xs->lock);
    __po_notify(xs, vf_spec);
    mutex_unlock(&xs->lock);
}

