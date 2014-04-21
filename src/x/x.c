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

const char *xprotocol_str[] = {
    "",
    "PF_NET",
    "PF_IPC",
    "PF_NET|PF_IPC",
    "PF_INPROC",
    "PF_NET|PF_INPROC",
    "PF_IPC|PF_INPROC",
    "PF_NET|PF_IPC|PF_INPROC",
};




void __xpoll_notify(struct xsock *sx, u32 l4proto_spec);
void xpoll_notify(struct xsock *sx, u32 l4proto_spec);

u32 xiov_len(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    return sizeof(msg->vec) + msg->vec.size;
}

char *xiov_base(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
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
    return msg->vec.chunk;
}

void xfreemsg(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    mem_free(msg, sizeof(*msg) + msg->vec.size);
}

u32 xmsglen(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
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

static int xshutdown_task_f(struct xtask *ts);

static void xsock_init(int xd) {
    struct xsock *sx = xget(xd);

    mutex_init(&sx->lock);
    condition_init(&sx->cond);
    ZERO(sx->addr);
    ZERO(sx->peer);
    sx->fasync = false;
    sx->fok = true;
    sx->fclosed = false;
    sx->parent = -1;
    sx->xd = xd;
    sx->cpu_no = choose_backend_poll(xd);
    sx->rcv_waiters = 0;
    sx->snd_waiters = 0;
    sx->rcv = 0;
    sx->snd = 0;
    sx->rcv_wnd = DEF_RCVBUF;
    sx->snd_wnd = DEF_SNDBUF;
    INIT_LIST_HEAD(&sx->rcv_head);
    INIT_LIST_HEAD(&sx->snd_head);
    INIT_LIST_HEAD(&sx->xpoll_head);
    sx->shutdown.f = xshutdown_task_f;
    INIT_LIST_HEAD(&sx->shutdown.link);
    INIT_LIST_HEAD(&sx->link);
    condition_init(&sx->accept_cond);
    sx->accept_waiters = 0;
    INIT_LIST_HEAD(&sx->request_socks);
}

static void xsock_exit(int xd) {
    struct xsock *sx = xget(xd);
    struct list_head head = {};
    struct xmsg *pos, *nx;

    mutex_destroy(&sx->lock);
    condition_destroy(&sx->cond);
    sx->ty = -1;
    sx->pf = -1;
    sx->fasync = 0;
    sx->fok = 0;
    sx->fclosed = 0;
    sx->xd = -1;
    sx->cpu_no = -1;
    sx->rcv_waiters = -1;
    sx->snd_waiters = -1;
    sx->rcv = -1;
    sx->snd = -1;
    sx->rcv_wnd = -1;
    sx->snd_wnd = -1;

    INIT_LIST_HEAD(&head);
    list_splice(&sx->rcv_head, &head);
    list_splice(&sx->snd_head, &head);
    xmsg_walk_safe(pos, nx, &head) {
	xfreemsg(pos->vec.chunk);
    }
    BUG_ON(attached(&sx->link));

    /* It's possible that user call xclose() and xpoll_add()
     * at the same time. and attach_to_xsock() happen after xclose().
     * this is a user's bug.
     */
    BUG_ON(!list_empty(&sx->xpoll_head));
    sx->accept_waiters = -1;
    condition_destroy(&sx->accept_cond);
    
    /* TODO: destroy request_socks's connection */
}


struct xsock *xsock_alloc() {
    int xd = xd_alloc();
    struct xsock *sx = xget(xd);
    xsock_init(xd);
    return sx;
}

void xsock_free(struct xsock *sx) {
    int xd = sx->xd;
    xsock_exit(xd);
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

static void xshutdown(struct xsock *sx) {
    struct xcpu *cpu = xcpuget(sx->cpu_no);
    struct xtask *ts = &sx->shutdown;    
    
    spin_lock(&cpu->lock);
    if (!sx->fclosed && !attached(&ts->link)) {
	sx->fclosed = true;
	list_add_tail(&ts->link, &cpu->shutdown_socks);
    }
    efd_signal(&cpu->efd);
    spin_unlock(&cpu->lock);
}

static int xshutdown_task_f(struct xtask *ts) {
    struct xsock *sx = cont_of(ts, struct xsock, shutdown);

    DEBUG_OFF("xsock %d shutdown protocol %s", sx->xd, xprotocol_str[sx->pf]);
    sx->l4proto->destroy(sx->xd);
    return 0;
}

static int __shutdown_socks_task_hndl(struct xcpu *cpu) {
    struct xtask *ts, *nx_ts;
    struct list_head st_head;

    INIT_LIST_HEAD(&st_head);
    spin_lock(&cpu->lock);
    efd_unsignal(&cpu->efd);
    list_splice(&cpu->shutdown_socks, &st_head);
    spin_unlock(&cpu->lock);

    xtask_walk_safe(ts, nx_ts, &st_head) {
	list_del_init(&ts->link);
	ts->f(ts);
    }
    return 0;
}

static int cpu_task_hndl(eloop_t *el, ev_t *et) {
    return 0;
}

static inline int kcpud(void *args) {
    waitgroup_t *wg = (waitgroup_t *)args;
    int rc = 0;
    int cpu_no = xcpu_alloc();
    struct xcpu *cpu = xcpuget(cpu_no);

    spin_init(&cpu->lock);
    INIT_LIST_HEAD(&cpu->shutdown_socks);

    /* Init eventloop and wakeup parent */
    BUG_ON(eloop_init(&cpu->el, XSOCK_MAX_SOCKS/XSOCK_MAX_CPUS,
		      DEF_ELOOPIOMAX, DEF_ELOOPTIMEOUT) != 0);
    BUG_ON(efd_init(&cpu->efd));
    cpu->efd_et.events = EPOLLIN|EPOLLERR;
    cpu->efd_et.fd = cpu->efd.r;
    cpu->efd_et.f = cpu_task_hndl;
    cpu->efd_et.data = cpu;
    BUG_ON(eloop_add(&cpu->el, &cpu->efd_et) != 0);

    /* Init done. wakeup parent thread */
    waitgroup_done(wg);

    while (!xgb.exiting) {
	eloop_once(&cpu->el);
	__shutdown_socks_task_hndl(cpu);
    }

    /* Release the poll descriptor when kcpud exit. */
    xcpu_free(cpu_no);
    eloop_destroy(&cpu->el);
    spin_destroy(&cpu->lock);
    return rc;
}


extern struct xsock_protocol xinproc_listener_protocol;
extern struct xsock_protocol xinproc_connector_protocol;
extern struct xsock_protocol xipc_listener_protocol;
extern struct xsock_protocol xipc_connector_protocol;
extern struct xsock_protocol xtcp_listener_protocol;
extern struct xsock_protocol xtcp_connector_protocol;


struct xsock_protocol *l4proto_lookup(int pf, int type) {
    struct xsock_protocol *l4proto, *nx;

    xsock_protocol_walk_safe(l4proto, nx, &xgb.xsock_protocol_head) {
	if (pf == l4proto->pf && l4proto->type == type)
	    return l4proto;
    }
    return 0;
}


void xmodule_init() {
    waitgroup_t wg;
    int xd;
    int cpu_no;
    int i;
    struct list_head *protocol_head = &xgb.xsock_protocol_head;
    
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
	taskpool_run(&xgb.tpool, kcpud, &wg);
    /* Waiting all poll's kcpud start properly */
    waitgroup_wait(&wg);
    waitgroup_destroy(&wg);
    
    /* The priority of xsock_protocol: inproc > ipc > tcp */
    INIT_LIST_HEAD(protocol_head);
    list_add_tail(&xinproc_listener_protocol.link, protocol_head);
    list_add_tail(&xinproc_connector_protocol.link, protocol_head);
    list_add_tail(&xipc_listener_protocol.link, protocol_head);
    list_add_tail(&xipc_connector_protocol.link, protocol_head);
    list_add_tail(&xtcp_listener_protocol.link, protocol_head);
    list_add_tail(&xtcp_connector_protocol.link, protocol_head);
}

void xmodule_exit() {
    xgb.exiting = true;
    taskpool_stop(&xgb.tpool);
    taskpool_destroy(&xgb.tpool);
    mutex_destroy(&xgb.lock);
}

void xclose(int xd) {
    struct xsock *sx = xget(xd);
    struct xpoll_t *po;
    struct xpoll_entry *ent, *nx;
    
    mutex_lock(&sx->lock);
    xsock_walk_ent(ent, nx, &sx->xpoll_head) {
	po = cont_of(ent->notify, struct xpoll_t, notify);
	xpoll_ctl(po, XPOLL_DEL, &ent->event);
	__detach_from_xsock(ent);
	xent_put(ent);
    }
    mutex_unlock(&sx->lock);

    /* Let backend thread do the last destroy() */
    xshutdown(sx);
}


int push_request_sock(struct xsock *sx, struct xsock *req_sx) {
    int rc = 0;
    mutex_lock(&sx->lock);
    if (list_empty(&sx->request_socks) && sx->accept_waiters > 0) {
	condition_broadcast(&sx->accept_cond);
    }
    list_add_tail(&req_sx->link, &sx->request_socks);
    __xpoll_notify(sx, XPOLLIN);
    mutex_unlock(&sx->lock);
    return rc;
}

struct xsock *pop_request_sock(struct xsock *sx) {
    struct xsock *req_sx = 0;

    mutex_lock(&sx->lock);
    while (list_empty(&sx->request_socks) && !sx->fasync) {
	sx->accept_waiters++;
	condition_wait(&sx->accept_cond, &sx->lock);
	sx->accept_waiters--;
    }
    if (!list_empty(&sx->request_socks)) {
	req_sx = list_first(&sx->request_socks, struct xsock, link);
	list_del_init(&req_sx->link);
    }
    mutex_unlock(&sx->lock);
    return req_sx;
}

int xaccept(int xd) {
    int rc = -1;
    struct xsock *sx = xget(xd);
    struct xsock *new = 0;

    if (!sx->fok) {
	errno = EPIPE;
	return -1;
    }
    errno = EAGAIN;
    if ((new = pop_request_sock(sx)))
	rc = new->xd;
    return rc;
}


int xlisten(int pf, const char *addr) {
    struct xsock *new;
    struct xsock_protocol *l4proto = l4proto_lookup(pf, XLISTENER);

    if (!l4proto) {
	errno = EPROTO;
	return -1;
    }
    new = xsock_alloc();
    new->pf = pf;
    new->l4proto = l4proto;
    strncpy(new->addr, addr, TP_SOCKADDRLEN);
    if (l4proto->init(new->xd) < 0) {
	xsock_free(new);
	return -1;
    }
    return new->xd;
}

int xconnect(int pf, const char *peer) {
    struct xsock *new;
    struct xsock_protocol *l4proto = l4proto_lookup(pf, XCONNECTOR);

    if (!l4proto) {
	errno = EPROTO;
	return -1;
    }
    new = xsock_alloc();
    new->pf = pf;
    new->l4proto = l4proto;
    strncpy(new->peer, peer, TP_SOCKADDRLEN);
    if (l4proto->init(new->xd) < 0) {
	xsock_free(new);
	return -1;
    }
    return new->xd;
}

int xsetopt(int xd, int opt, void *on, int size) {
    int rc = 0;
    struct xsock *sx = xget(xd);

    if (!on || size <= 0) {
	errno = EINVAL;
	return -1;
    }
    switch (opt) {
    case XNOBLOCK:
	mutex_lock(&sx->lock);
	sx->fasync = *(int *)on ? true : false;
	mutex_unlock(&sx->lock);
	break;
    case XSNDBUF:
	mutex_lock(&sx->lock);
	sx->snd_wnd = (*(int *)on);
	mutex_unlock(&sx->lock);
	break;
    case XRCVBUF:
	mutex_lock(&sx->lock);
	sx->rcv_wnd = (*(int *)on);
	mutex_unlock(&sx->lock);
	break;
    default:
	errno = EINVAL;
	return -1;
    }
    return rc;
}

int xgetopt(int xd, int opt, void *on, int size) {
    int rc = 0;
    struct xsock *sx = xget(xd);

    if (!on || size <= 0) {
	errno = EINVAL;
	return -1;
    }
    switch (opt) {
    case XNOBLOCK:
	mutex_lock(&sx->lock);
	*(int *)on = sx->fasync ? true : false;
	mutex_unlock(&sx->lock);
	break;
    case XSNDBUF:
	mutex_lock(&sx->lock);
	*(int *)on = sx->snd_wnd;
	mutex_unlock(&sx->lock);
	break;
    case XRCVBUF:
	mutex_lock(&sx->lock);
	*(int *)on = sx->rcv_wnd;
	mutex_unlock(&sx->lock);
	break;
    default:
	errno = EINVAL;
	return -1;
    }
    return rc;
}


struct xmsg *pop_rcv(struct xsock *sx) {
    struct xmsg *msg = 0;
    struct xsock_protocol *l4proto = sx->l4proto;
    i64 msgsz;
    u32 events = 0;

    mutex_lock(&sx->lock);
    while (list_empty(&sx->rcv_head) && !sx->fasync) {
	sx->rcv_waiters++;
	condition_wait(&sx->cond, &sx->lock);
	sx->rcv_waiters--;
    }
    if (!list_empty(&sx->rcv_head)) {
	DEBUG_OFF("xsock %d", sx->xd);
	msg = list_first(&sx->rcv_head, struct xmsg, item);
	list_del_init(&msg->item);
	msgsz = xiov_len(msg->vec.chunk);
	sx->rcv -= msgsz;
	events |= XMQ_POP;
	if (sx->rcv_wnd - sx->rcv <= msgsz)
	    events |= XMQ_NONFULL;
	if (list_empty(&sx->rcv_head)) {
	    BUG_ON(sx->rcv);
	    events |= XMQ_EMPTY;
	}
    }

    if (events && l4proto->rcv_notify)
	l4proto->rcv_notify(sx->xd, events);

    mutex_unlock(&sx->lock);
    return msg;
}

void push_rcv(struct xsock *sx, struct xmsg *msg) {
    struct xsock_protocol *l4proto = sx->l4proto;
    u32 events = 0;
    i64 msgsz = xiov_len(msg->vec.chunk);

    mutex_lock(&sx->lock);
    if (list_empty(&sx->rcv_head))
	events |= XMQ_NONEMPTY;
    if (sx->rcv_wnd - sx->rcv <= msgsz)
	events |= XMQ_FULL;
    events |= XMQ_PUSH;
    sx->rcv += msgsz;
    list_add_tail(&msg->item, &sx->rcv_head);    
    __xpoll_notify(sx, 0);
    DEBUG_OFF("xsock %d", sx->xd);

    /* Wakeup the blocking waiters. */
    if (sx->rcv_waiters > 0)
	condition_broadcast(&sx->cond);

    if (events && l4proto->rcv_notify)
	l4proto->rcv_notify(sx->xd, events);
    mutex_unlock(&sx->lock);
}


struct xmsg *pop_snd(struct xsock *sx) {
    struct xsock_protocol *l4proto = sx->l4proto;
    struct xmsg *msg = 0;
    i64 msgsz;
    u32 events = 0;
    
    mutex_lock(&sx->lock);
    if (!list_empty(&sx->snd_head)) {
	DEBUG_OFF("xsock %d", sx->xd);
	msg = list_first(&sx->snd_head, struct xmsg, item);
	list_del_init(&msg->item);
	msgsz = xiov_len(msg->vec.chunk);
	sx->snd -= msgsz;
	events |= XMQ_POP;
	if (sx->snd_wnd - sx->snd <= msgsz)
	    events |= XMQ_NONFULL;
	if (list_empty(&sx->snd_head)) {
	    BUG_ON(sx->snd);
	    events |= XMQ_EMPTY;
	}

	/* Wakeup the blocking waiters */
	if (sx->snd_waiters > 0)
	    condition_broadcast(&sx->cond);
    }

    if (events && l4proto->snd_notify)
	l4proto->snd_notify(sx->xd, events);

    __xpoll_notify(sx, 0);
    mutex_unlock(&sx->lock);
    return msg;
}

int push_snd(struct xsock *sx, struct xmsg *msg) {
    int rc = -1;
    struct xsock_protocol *l4proto = sx->l4proto;
    u32 events = 0;
    i64 msgsz = xiov_len(msg->vec.chunk);

    mutex_lock(&sx->lock);
    while (!can_send(sx) && !sx->fasync) {
	sx->snd_waiters++;
	condition_wait(&sx->cond, &sx->lock);
	sx->snd_waiters--;
    }
    if (can_send(sx)) {
	rc = 0;
	if (list_empty(&sx->snd_head))
	    events |= XMQ_NONEMPTY;
	if (sx->snd_wnd - sx->snd <= msgsz)
	    events |= XMQ_FULL;
	events |= XMQ_PUSH;
	sx->snd += msgsz;
	list_add_tail(&msg->item, &sx->snd_head);
	DEBUG_OFF("xsock %d", sx->xd);
    }

    if (events && l4proto->snd_notify)
	l4proto->snd_notify(sx->xd, events);

    mutex_unlock(&sx->lock);
    return rc;
}

int xrecv(int xd, char **xbuf) {
    int rc = 0;
    struct xsock *sx = xget(xd);
    struct xmsg *msg;

    if (!xbuf) {
	errno = EINVAL;
	return -1;
    }
    if (!(msg = pop_rcv(sx))) {
	errno = sx->fok ? EAGAIN : EPIPE;
	rc = -1;
    } else
	*xbuf = msg->vec.chunk;
    return rc;
}

int xsend(int xd, char *xbuf) {
    int rc = 0;
    struct xmsg *msg;
    struct xsock *sx = xget(xd);

    if (!xbuf) {
	errno = EINVAL;
	return -1;
    }
    msg = cont_of(xbuf, struct xmsg, vec.chunk);
    if ((rc = push_snd(sx, msg)) < 0) {
	errno = sx->fok ? EAGAIN : EPIPE;
    }
    return rc;
}


/* Generic xpoll_t notify function. always called by xsock_protocol
 * when has any message come or can send any massage into network
 * or has a new connection wait for established.
 * here we only check the mq events and l4proto_spec saved the other
 * events gived by xsock_protocol
 */
void __xpoll_notify(struct xsock *sx, u32 l4proto_spec) {
    int events = 0;
    struct xpoll_entry *ent, *nx;

    events |= l4proto_spec;
    events |= !list_empty(&sx->rcv_head) ? XPOLLIN : 0;
    events |= can_send(sx) ? XPOLLOUT : 0;
    events |= !sx->fok ? XPOLLERR : 0;
    DEBUG_OFF("%d xsock events %d happen", sx->xd, events);
    xsock_walk_ent(ent, nx, &sx->xpoll_head) {
	ent->notify->event(ent->notify, ent, ent->event.care & events);
    }
}

void xpoll_notify(struct xsock *sx, u32 l4proto_spec) {
    mutex_lock(&sx->lock);
    __xpoll_notify(sx, l4proto_spec);
    mutex_unlock(&sx->lock);
}

