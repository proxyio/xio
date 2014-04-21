#include "xbase.h"

static int xmul_listener_destroy(int xd);

static int xmul_listener_init(int pf, const char *sock) {
    struct xsock *sx = 0;
    int sub_xd;
    struct xsock *sub_sx = 0;
    struct xpoll_event ev = {};
    struct xsock_protocol *l4proto, *nx;
    struct list_head *protocol_head = &xgb.xsock_protocol_head;
    
    if (!(sx = xsock_alloc())) {
	errno = EAGAIN;
	return -1;
    }
    if (!(sx->mul.poll = xpoll_create())) {
	xsock_free(sx);
	errno = EAGAIN;
	return -1;
    }
    DEBUG_OFF("%s", xprotocol_str[pf]);
    sx->pf = pf;
    strncpy(sx->addr, sock, TP_SOCKADDRLEN);
    INIT_LIST_HEAD(&sx->mul.listen_head);

    xsock_protocol_walk_safe(l4proto, nx, protocol_head) {
	if ((pf & l4proto->pf) != l4proto->pf || XLISTENER != l4proto->type)
	    continue;
	if ((sub_xd = xlisten(pf & l4proto->pf, sock)) < 0) {
	BAD:
	    xmul_listener_destroy(sx->xd);
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
    return sx->xd;
}

static int xmul_listener_destroy(int xd) {
    struct xsock *sx = xget(xd);
    struct xsock *sub_sx, *nx_sx;

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
    .type = XLISTENER,
    .pf = PF_NET|PF_INPROC|PF_IPC,
    .init = xmul_listener_init,
    .destroy = null,
    .snd_notify = null,
    .rcv_notify = null,
};
