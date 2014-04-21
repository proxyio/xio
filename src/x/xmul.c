#include "xbase.h"

static int xmul_accepter_init(int xd) {
    int rc = 0;
    return rc;
}

static int xmul_listener_destroy(int xd);

static int xmul_listener_init(int xd) {
    struct xsock *sx = xget(xd);
    int pf = sx->pf;
    int sub_xd;
    struct xsock *sub_sx;
    struct xpoll_event ev = {};
    struct xsock_protocol *l4proto, *nx;

    DEBUG_OFF("%s", xprotocol_str[pf]);
    sx->mul.poll = xpoll_create();
    BUG_ON(!sx->mul.poll);
    xsock_protocol_walk_safe(l4proto, nx, &xgb.xsock_protocol_head) {
	if ((pf & l4proto->pf) != l4proto->pf)
	    continue;
	if ((sub_xd = xlisten(pf & l4proto->pf, sx->addr)) < 0) {
	BAD:
	    xmul_listener_destroy(xd);
	    return -1;
	}
	sub_sx = xget(sub_xd);
	ev.xd = sub_xd;
	ev.self = sub_sx;
	ev.care = XPOLLIN|XPOLLERR;
	if (xpoll_ctl(sx->mul.poll, XPOLL_ADD, &ev) < 0)
	    goto BAD;
	pf &= ~l4proto->pf;
	list_add_tail(&sub_sx->link, &sx->mul.listen_head);
    }
    if (pf)
	goto BAD;
    return 0;
}

static int xmul_listener_destroy(int xd) {
    struct xsock *sx = xget(xd);
    struct xsock *sub_sx, *nx_sx;

    DEBUG_OFF("%s", xprotocol_str[sx->pf]);
    xsock_walk_safe(sub_sx, nx_sx, &sx->mul.listen_head) {
	list_del_init(&sub_sx->link);
	xclose(sub_sx->xd);
    }
    BUG_ON(!list_empty(&sx->mul.listen_head));
    if (sx->mul.poll)
	xpoll_close(sx->mul.poll);
    return 0;
}

struct xsock_protocol ipc_inp_net_xsock_protocol = {
    .pf = PF_NET|PF_INPROC|PF_IPC,
    .init = null,
    .destroy = null,
    .snd_notify = null,
    .rcv_notify = null,
};
