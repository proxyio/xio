#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "xsock_struct.h"

extern int _xlisten(int pf, const char *addr);


static void xmultiple_close(int xd) {
    struct xsock *sub_sx, *nx;
    struct xsock *sx = xget(xd);

    xsock_walk_sub_socks(sub_sx, nx, &sx->sub_socks) {
	sub_sx->parent = -1;
	list_del_init(&sub_sx->sib_link);
	xclose(sub_sx->xd);
    }
}

static int xmul_listener_bind(int xd, const char *sock) {
    struct xsock_protocol *l4proto, *nx;
    struct xsock *sx = xget(xd), *sub_sx;
    int sub_xd;
    int pf = sx->pf;

    xsock_protocol_walk_safe(l4proto, nx, &xgb.xsock_protocol_head) {
	if (!(pf & l4proto->pf) || l4proto->type != XLISTENER)
	    continue;
	pf &= ~l4proto->pf;
	if ((sub_xd = _xlisten(l4proto->pf, sock)) < 0)
	    goto BAD;
	sub_sx = xget(sub_xd);
	sub_sx->parent = xd;
	list_add_tail(&sub_sx->sib_link, &sx->sub_socks);
    }
    if (!list_empty(&sx->sub_socks))
	return 0;
 BAD:
    xmultiple_close(xd);
    return -1;
}

static void xmul_listener_close(int xd) {
    struct xsock *sx = xget(xd);
    xmultiple_close(xd);
    xsock_free(sx);
    DEBUG_OFF("xsock %d multiple_close", xd);
}

struct xsock_protocol xmul_listener_protocol[4] = {
    {
	.type = XLISTENER,
	.pf = XPF_TCP|XPF_IPC,
	.bind = xmul_listener_bind,
	.close = xmul_listener_close,
	.rcv_notify = null,
	.snd_notify = null,
    },
    {
	.type = XLISTENER,
	.pf = XPF_IPC|XPF_INPROC,
	.bind = xmul_listener_bind,
	.close = xmul_listener_close,
	.rcv_notify = null,
	.snd_notify = null,
    },
    {
	.type = XLISTENER,
	.pf = XPF_TCP|XPF_INPROC,
	.bind = xmul_listener_bind,
	.close = xmul_listener_close,
	.rcv_notify = null,
	.snd_notify = null,
    },
    {
	.type = XLISTENER,
	.pf = XPF_TCP|XPF_IPC|XPF_INPROC,
	.bind = xmul_listener_bind,
	.close = xmul_listener_close,
	.rcv_notify = null,
	.snd_notify = null,
    }
};
