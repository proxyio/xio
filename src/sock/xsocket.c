#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sync/waitgroup.h>
#include "runner/taskpool.h"
#include "xsock_struct.h"

static void xshutdown(struct xsock *sx) {
    struct xcpu *cpu = xcpuget(sx->cpu_no);
    struct xtask *ts = &sx->shutdown;    

    mutex_lock(&cpu->lock);
    while (efd_signal(&cpu->efd) < 0) {
	/* Pipe is full and another thread is unsignaling. we release */
	mutex_unlock(&cpu->lock);
	mutex_lock(&cpu->lock);
    }
    if (!sx->fclosed && !attached(&ts->link)) {
	sx->fclosed = true;
	list_add_tail(&ts->link, &cpu->shutdown_socks);
    }
    mutex_unlock(&cpu->lock);
}

void xshutdown_task_f(struct xtask *ts) {
    struct xsock *sx = cont_of(ts, struct xsock, shutdown);

    DEBUG_OFF("xsock %d shutdown protocol %s", sx->xd, xprotocol_str[sx->pf]);
    sx->l4proto->close(sx->xd);
    return 0;
}

void xclose(int xd) {
    struct xsock *sx = xget(xd);
    struct xpoll_t *po;
    struct xpoll_entry *ent, *nx;
    struct list_head xpoll_head = {};

    INIT_LIST_HEAD(&xpoll_head);
    mutex_lock(&sx->lock);
    list_splice(&sx->xpoll_head, &xpoll_head);
    mutex_unlock(&sx->lock);

    xsock_walk_ent(ent, nx, &xpoll_head) {
	po = cont_of(ent->notify, struct xpoll_t, notify);
	xpoll_ctl(po, XPOLL_DEL, &ent->event);
	__detach_from_xsock(ent);
	xent_put(ent);
    }

    /* Let backend thread do the last destroy(). */
    xshutdown(sx);
}

int xsocket(int pf, int type) {
    struct xsock *sx = xsock_alloc();

    if (!sx) {
	errno = EMFILE;
	return -1;
    }
    if (!(sx->l4proto = l4proto_lookup(pf, type))) {
	errno = EPROTO;
	return -1;
    }
    sx->pf = pf;
    sx->type = type;
    return sx->xd;
}

int xbind(int xd, const char *addr) {
    int rc;
    struct xsock *sx = xget(xd);

    if (sx->l4proto)
	rc = sx->l4proto->bind(xd, addr);
    return rc;
}
