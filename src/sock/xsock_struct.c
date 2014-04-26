#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sync/waitgroup.h>
#include "runner/taskpool.h"
#include "xbase.h"

/* Default input/output buffer size */
static int DEF_SNDBUF = 10485760;
static int DEF_RCVBUF = 10485760;

u32 xiov_len(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    return sizeof(msg->vec) + msg->vec.size;
}

char *xiov_base(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    return (char *)&msg->vec;
}

char *xallocmsg(int size) {
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

int xmsglen(char *xbuf) {
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

extern int xshutdown_task_f(struct xtask *ts);

static void xsock_init(int xd) {
    struct xsock *sx = xget(xd);

    mutex_init(&sx->lock);
    condition_init(&sx->cond);
    sx->type = 0;
    sx->pf = 0;
    ZERO(sx->addr);
    ZERO(sx->peer);
    sx->fasync = false;
    sx->fok = true;
    sx->fclosed = false;

    sx->parent = -1;
    INIT_LIST_HEAD(&sx->sub_socks);
    INIT_LIST_HEAD(&sx->sib_link);
    
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
    sx->type = -1;
    sx->pf = -1;
    ZERO(sx->addr);
    ZERO(sx->peer);
    sx->fasync = 0;
    sx->fok = 0;
    sx->fclosed = 0;

    sx->parent = -1;
    BUG_ON(!list_empty(&sx->sub_socks));
    BUG_ON(attached(&sx->sib_link));
    
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
