#include "xbase.h"

static int xmul_accepter_init(int xd) {
    int rc;
    struct xsock *new = xget(xd), *sub_sx;
    struct xsock *parent = xget(new->parent);
    struct xpoll_event ev;
    u32 to = parent->fasync ? 0 : ~0;
    
    if ((rc = xpoll_wait(parent->mul.poll, &ev, 1, to)) <= 0)
	return -1;
    BUG_ON(rc != 1);
    sub_sx = (struct xsock *)ev.self;
    DEBUG_OFF("xsock %d %s ready for accept %s", sub_sx->xd,
	      xprotocol_str[sub_sx->pf], xpoll_str[ev.happened]);
    new->pf = sub_sx->pf;
    new->parent = sub_sx->xd;
    new->l4proto = sub_sx->l4proto;
    rc = new->l4proto->init(xd);
    xpoll_notify(sub_sx, 0);
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


static int xmul_init(int xd) {
    struct xsock *sx = xget(xd);

    INIT_LIST_HEAD(&sx->mul.listen_head);
    sx->mul.poll = 0;

    switch (sx->ty) {
    case XLISTENER:
	return xmul_listener_init(xd);
    }
    errno = EINVAL;
    return -1;
}

static void xmul_destroy(int xd) {
    struct xsock *sx = xget(xd);

    switch (sx->ty) {
    case XLISTENER:
	xmul_listener_destroy(xd);
	break;
    default:
	BUG_ON(1);
    }
}

struct xsock_protocol ipc_and_inp_xsock_protocol = {
    .pf = PF_INPROC|PF_IPC,
    .init = xmul_init,
    .destroy = xmul_destroy,
    .snd_notify = null,
    .rcv_notify = null,
};

struct xsock_protocol ipc_and_net_xsock_protocol = {
    .pf = PF_NET|PF_IPC,
    .init = xmul_init,
    .destroy = xmul_destroy,
    .snd_notify = null,
    .rcv_notify = null,
};

struct xsock_protocol net_and_inp_xsock_protocol = {
    .pf = PF_NET|PF_INPROC,
    .init = xmul_init,
    .destroy = xmul_destroy,
    .snd_notify = null,
    .rcv_notify = null,
};

struct xsock_protocol ipc_inp_net_xsock_protocol = {
    .pf = PF_NET|PF_INPROC|PF_IPC,
    .init = xmul_init,
    .destroy = xmul_destroy,
    .snd_notify = null,
    .rcv_notify = null,
};
