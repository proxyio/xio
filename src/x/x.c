#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sync/waitgroup.h>
#include "runner/taskpool.h"
#include "xbase.h"

/* Backend poller wait kernel timeout msec */
#define DEF_ELOOPTIMEOUT 1
#define DEF_ELOOPIOMAX 100

/* Default input/output buffer size */
static int DEF_SNDBUF = 10485760;
static int DEF_RCVBUF = 10485760;


struct xglobal xgb = {};


extern int has_closed_xsock(struct xcpu *cpu);
extern void push_closed_xsock(struct xsock *xs);
extern struct xsock *pop_closed_xsock(struct xcpu *cpu);

void __xpoll_notify(struct xsock *xs, u32 vf_spec);
void xpoll_notify(struct xsock *xs, u32 vf_spec);

u32 xiov_len(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.payload);
    return sizeof(msg->vec) + msg->vec.size;
}

char *xiov_base(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.payload);
    return (char *)&msg->vec;
}

char *xallocmsg(u32 size) {
    struct xmsg *msg;
    char *chunk = (char *)mem_zalloc(sizeof(*msg) + size);
    if (!chunk)
	return null;
    msg = (struct xmsg *)chunk;
    msg->vec.size = size;
    msg->vec.checksum = crc16((char *)&msg->vec.size, 4);
    return msg->vec.payload;
}

void xfreemsg(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.payload);
    mem_free(msg, sizeof(*msg) + msg->vec.size);
}

u32 xmsglen(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.payload);
    return msg->vec.size;
}


static int choose_backend_poll(int xd) {
    return xd % xgb.ncpus;
}

int xd_alloc() {
    int xd;
    mutex_lock(&xgb.lock);
    BUG_ON(xgb.nsocks >= XSOCK_MAX_SOCKS);
    xd = xgb.unused[xgb.nsocks++];
    mutex_unlock(&xgb.lock);
    return xd;
}

void xd_free(int xd) {
    mutex_lock(&xgb.lock);
    xgb.unused[--xgb.nsocks] = xd;
    mutex_unlock(&xgb.lock);
}

struct xsock *xget(int xd) {
    return &xgb.socks[xd];
}

static void xbase_init(int xd) {
    struct xsock *xs = xget(xd);

    mutex_init(&xs->lock);
    condition_init(&xs->cond);
    xs->fasync = false;
    xs->fok = true;
    xs->fclosed = false;
    xs->parent = -1;
    xs->xd = xd;
    xs->cpu_no = choose_backend_poll(xd);
    xs->rcv_waiters = 0;
    xs->snd_waiters = 0;
    xs->rcv = 0;
    xs->snd = 0;
    xs->rcv_wnd = DEF_RCVBUF;
    xs->snd_wnd = DEF_SNDBUF;
    INIT_LIST_HEAD(&xs->rcv_head);
    INIT_LIST_HEAD(&xs->snd_head);
    INIT_LIST_HEAD(&xs->xpoll_head);
    INIT_LIST_HEAD(&xs->closing_link);
}

static void xbase_exit(int xd) {
    struct xsock *xs = xget(xd);
    struct list_head head = {};
    struct xmsg *pos, *nx;

    mutex_destroy(&xs->lock);
    condition_destroy(&xs->cond);
    xs->ty = -1;
    xs->pf = -1;
    xs->fasync = 0;
    xs->fok = 0;
    xs->fclosed = 0;
    xs->xd = -1;
    xs->cpu_no = -1;
    xs->rcv_waiters = -1;
    xs->snd_waiters = -1;
    xs->rcv = -1;
    xs->snd = -1;
    xs->rcv_wnd = -1;
    xs->snd_wnd = -1;

    INIT_LIST_HEAD(&head);
    list_splice(&xs->rcv_head, &head);
    list_splice(&xs->snd_head, &head);
    xmsg_walk_safe(pos, nx, &head) {
	xfreemsg(pos->vec.payload);
    }
    BUG_ON(attached(&xs->closing_link));

    /* It's possible that user call xclose() and xpoll_add()
     * at the same time. and attach_to_xsock() happen after xclose().
     * this is a user's bug.
     */
    BUG_ON(!list_empty(&xs->xpoll_head));
}


struct xsock *xsock_alloc() {
    int xd = xd_alloc();
    struct xsock *xs = xget(xd);
    xbase_init(xd);
    return xs;
}

void xsock_free(struct xsock *xs) {
    int xd = xs->xd;
    xbase_exit(xd);
    xd_free(xd);
}

int xcpu_alloc() {
    int cpu_no;
    mutex_lock(&xgb.lock);
    BUG_ON(xgb.ncpus >= XSOCK_MAX_CPUS);
    cpu_no = xgb.cpu_unused[xgb.ncpus++];
    mutex_unlock(&xgb.lock);
    return cpu_no;
}

void xcpu_free(int cpu_no) {
    mutex_lock(&xgb.lock);
    xgb.cpu_unused[--xgb.ncpus] = cpu_no;
    mutex_unlock(&xgb.lock);
}

struct xcpu *xcpuget(int cpu_no) {
    return &xgb.cpus[cpu_no];
}


int has_closed_xsock(struct xcpu *cpu) {
    int has = true;
    spin_lock(&cpu->lock);
    if (list_empty(&cpu->closing_head))
	has = false;
    spin_unlock(&cpu->lock);
    return has;
}

void push_closed_xsock(struct xsock *xs) {
    struct xcpu *cpu = xcpuget(xs->cpu_no);
    
    spin_lock(&cpu->lock);
    if (!xs->fclosed && !attached(&xs->closing_link)) {
	xs->fclosed = true;
	list_add_tail(&xs->closing_link, &cpu->closing_head);
    }
    spin_unlock(&cpu->lock);
}

struct xsock *pop_closed_xsock(struct xcpu *cpu) {
    struct xsock *xs = 0;

    spin_lock(&cpu->lock);
    if (!list_empty(&cpu->closing_head)) {
	xs = list_first(&cpu->closing_head, struct xsock, closing_link);
	list_del_init(&xs->closing_link);
    }
    spin_unlock(&cpu->lock);
    return xs;
}

static inline int event_runner(void *args) {
    waitgroup_t *wg = (waitgroup_t *)args;
    int rc = 0;
    int cpu_no = xcpu_alloc();
    struct xsock *closing_xs;
    struct xcpu *cpu = xcpuget(cpu_no);

    spin_init(&cpu->lock);
    INIT_LIST_HEAD(&cpu->closing_head);

    /* Init eventloop and wakeup parent */
    BUG_ON(eloop_init(&cpu->el, XSOCK_MAX_SOCKS/XSOCK_MAX_CPUS,
		      DEF_ELOOPIOMAX, DEF_ELOOPTIMEOUT) != 0);
    waitgroup_done(wg);

    while (!xgb.exiting || has_closed_xsock(cpu)) {
	eloop_once(&cpu->el);
	while ((closing_xs = pop_closed_xsock(cpu)))
	    closing_xs->vf->destroy(closing_xs->xd);
    }

    /* Release the poll descriptor when runner exit. */
    xcpu_free(cpu_no);
    eloop_destroy(&cpu->el);
    spin_destroy(&cpu->lock);
    return rc;
}


extern struct xsock_vf *inproc_xsock_vfptr;
extern struct xsock_vf *ipc_xsock_vfptr;
extern struct xsock_vf *tcp_xsock_vfptr;


void global_xinit() {
    waitgroup_t wg;
    int xd;
    int cpu_no;
    int i;

    waitgroup_init(&wg);
    xgb.exiting = false;
    mutex_init(&xgb.lock);

    for (xd = 0; xd < XSOCK_MAX_SOCKS; xd++)
	xgb.unused[xd] = xd;
    for (cpu_no = 0; cpu_no < XSOCK_MAX_CPUS; cpu_no++)
	xgb.cpu_unused[cpu_no] = cpu_no;

    xgb.cpu_cores = 2;
    taskpool_init(&xgb.tpool, xgb.cpu_cores);
    taskpool_start(&xgb.tpool);
    waitgroup_adds(&wg, xgb.cpu_cores);
    for (i = 0; i < xgb.cpu_cores; i++)
	taskpool_run(&xgb.tpool, event_runner, &wg);
    /* Waiting all poll's event_runner start properly */
    waitgroup_wait(&wg);
    waitgroup_destroy(&wg);
    
    /* The priority of xsock_vf: inproc > ipc > tcp */
    INIT_LIST_HEAD(&xgb.xsock_vf_head);
    list_add_tail(&inproc_xsock_vfptr->vf_item, &xgb.xsock_vf_head);
    list_add_tail(&ipc_xsock_vfptr->vf_item, &xgb.xsock_vf_head);
    list_add_tail(&tcp_xsock_vfptr->vf_item, &xgb.xsock_vf_head);
}

void global_xexit() {
    xgb.exiting = true;
    taskpool_stop(&xgb.tpool);
    taskpool_destroy(&xgb.tpool);
    mutex_destroy(&xgb.lock);
}

void xclose(int xd) {
    struct xsock *xs = xget(xd);
    struct xpoll_t *po;
    struct xpoll_entry *ent, *nx;
    
    mutex_lock(&xs->lock);
    xsock_walk_ent(ent, nx, &xs->xpoll_head) {
	po = cont_of(ent->notify, struct xpoll_t, notify);
	xpoll_ctl(po, XPOLL_DEL, &ent->event);
	__detach_from_xsock(ent);
	xent_put(ent);
    }
    mutex_unlock(&xs->lock);
    /* Let backend thread do the last destroy() */
    push_closed_xsock(xs);
}

int xaccept(int xd) {
    struct xsock *xs = xget(xd);
    struct xsock *new = xsock_alloc();
    struct xsock_vf *vf, *nx;

    if (!xs->fok) {
	errno = EPIPE;
	goto EXIT;
    }
    new->ty = XACCEPTER;
    new->pf = xs->pf;
    new->parent = xd;
    xpoll_notify(xs, 0);
    xsock_vf_walk_safe(vf, nx, &xgb.xsock_vf_head) {
	new->vf = vf;
	if ((xs->pf & vf->pf) && vf->init(new->xd) == 0) {
	    return new->xd;
	}
    }
 EXIT:
    DEBUG_OFF("%d failed with errno %d", xd, errno);
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
    xsock_vf_walk_safe(vf, nx, &xgb.xsock_vf_head) {
	new->vf = vf;
	if ((pf & vf->pf) && vf->init(new->xd) == 0)
	    return new->xd;
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
    xsock_vf_walk_safe(vf, nx, &xgb.xsock_vf_head) {
	new->vf = vf;
	if ((pf & vf->pf) && vf->init(new->xd) == 0) {
	    return new->xd;
	}
    }
    xsock_free(new);
    return -1;
}

int xsetopt(int xd, int opt, void *on, int size) {
    int rc = 0;
    struct xsock *xs = xget(xd);

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

int xgetopt(int xd, int opt, void *on, int size) {
    int rc = 0;
    struct xsock *xs = xget(xd);

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
    struct xmsg *msg = 0;
    struct xsock_vf *vf = xs->vf;
    i64 msgsz;
    u32 events = 0;

    mutex_lock(&xs->lock);
    while (list_empty(&xs->rcv_head) && !xs->fasync) {
	xs->rcv_waiters++;
	condition_wait(&xs->cond, &xs->lock);
	xs->rcv_waiters--;
    }
    if (!list_empty(&xs->rcv_head)) {
	DEBUG_OFF("channel %d", xs->xd);
	msg = list_first(&xs->rcv_head, struct xmsg, item);
	list_del_init(&msg->item);
	msgsz = xiov_len(msg->vec.payload);
	xs->rcv -= msgsz;
	events |= XMQ_POP;
	if (xs->rcv_wnd - xs->rcv <= msgsz)
	    events |= XMQ_NONFULL;
	if (list_empty(&xs->rcv_head)) {
	    BUG_ON(xs->rcv);
	    events |= XMQ_EMPTY;
	}
    }

    if (events)
	vf->rcv_notify(xs->xd, events);

    mutex_unlock(&xs->lock);
    return msg;
}

void push_rcv(struct xsock *xs, struct xmsg *msg) {
    struct xsock_vf *vf = xs->vf;
    u32 events = 0;
    i64 msgsz = xiov_len(msg->vec.payload);

    mutex_lock(&xs->lock);
    if (list_empty(&xs->rcv_head))
	events |= XMQ_NONEMPTY;
    if (xs->rcv_wnd - xs->rcv <= msgsz)
	events |= XMQ_FULL;
    events |= XMQ_PUSH;
    xs->rcv += msgsz;
    list_add_tail(&msg->item, &xs->rcv_head);    
    __xpoll_notify(xs, 0);
    DEBUG_OFF("channel %d", xs->xd);

    /* Wakeup the blocking waiters. */
    if (xs->rcv_waiters > 0)
	condition_broadcast(&xs->cond);

    if (events)
	vf->rcv_notify(xs->xd, events);
    mutex_unlock(&xs->lock);
}


struct xmsg *pop_snd(struct xsock *xs) {
    struct xsock_vf *vf = xs->vf;
    struct xmsg *msg = 0;
    i64 msgsz;
    u32 events = 0;
    
    mutex_lock(&xs->lock);
    if (!list_empty(&xs->snd_head)) {
	DEBUG_OFF("channel %d", xs->xd);
	msg = list_first(&xs->snd_head, struct xmsg, item);
	list_del_init(&msg->item);
	msgsz = xiov_len(msg->vec.payload);
	xs->snd -= msgsz;
	events |= XMQ_POP;
	if (xs->snd_wnd - xs->snd <= msgsz)
	    events |= XMQ_NONFULL;
	if (list_empty(&xs->snd_head)) {
	    BUG_ON(xs->snd);
	    events |= XMQ_EMPTY;
	}

	/* Wakeup the blocking waiters */
	if (xs->snd_waiters > 0)
	    condition_broadcast(&xs->cond);
    }

    if (events)
	vf->snd_notify(xs->xd, events);

    __xpoll_notify(xs, 0);
    mutex_unlock(&xs->lock);
    return msg;
}

int push_snd(struct xsock *xs, struct xmsg *msg) {
    int rc = -1;
    struct xsock_vf *vf = xs->vf;
    u32 events = 0;
    i64 msgsz = xiov_len(msg->vec.payload);

    mutex_lock(&xs->lock);
    while (!can_send(xs) && !xs->fasync) {
	xs->snd_waiters++;
	condition_wait(&xs->cond, &xs->lock);
	xs->snd_waiters--;
    }
    if (can_send(xs)) {
	rc = 0;
	if (list_empty(&xs->snd_head))
	    events |= XMQ_NONEMPTY;
	if (xs->snd_wnd - xs->snd <= msgsz)
	    events |= XMQ_FULL;
	events |= XMQ_PUSH;
	xs->snd += msgsz;
	list_add_tail(&msg->item, &xs->snd_head);
	DEBUG_OFF("channel %d", xs->xd);
    }

    if (events)
	vf->snd_notify(xs->xd, events);

    mutex_unlock(&xs->lock);
    return rc;
}

int xrecv(int xd, char **xbuf) {
    int rc = 0;
    struct xsock *xs = xget(xd);
    struct xmsg *msg;

    if (!xbuf) {
	errno = EINVAL;
	return -1;
    }
    if (!(msg = pop_rcv(xs))) {
	errno = xs->fok ? EAGAIN : EPIPE;
	rc = -1;
    } else
	*xbuf = msg->vec.payload;
    return rc;
}

int xsend(int xd, char *xbuf) {
    int rc = 0;
    struct xmsg *msg;
    struct xsock *xs = xget(xd);

    if (!xbuf) {
	errno = EINVAL;
	return -1;
    }
    msg = cont_of(xbuf, struct xmsg, vec.payload);
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
void __xpoll_notify(struct xsock *xs, u32 vf_spec) {
    int events = 0;
    struct xpoll_entry *ent, *nx;

    events |= vf_spec;
    events |= !list_empty(&xs->rcv_head) ? XPOLLIN : 0;
    events |= can_send(xs) ? XPOLLOUT : 0;
    events |= !xs->fok ? XPOLLERR : 0;
    DEBUG_OFF("%d channel xpoll events %d happen", xs->xd, events);
    xsock_walk_ent(ent, nx, &xs->xpoll_head) {
	ent->notify->event(ent->notify, ent, ent->event.care & events);
    }
}

void xpoll_notify(struct xsock *xs, u32 vf_spec) {
    mutex_lock(&xs->lock);
    __xpoll_notify(xs, vf_spec);
    mutex_unlock(&xs->lock);
}

