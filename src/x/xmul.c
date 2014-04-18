#include "xbase.h"


static int xmul_accepter_init(int xd) {
    struct xsock *sx, *nx_sx;
    struct xsock *new = xget(xd);
    struct xsock *parent = xget(new->parent);

    xsock_walk_safe(sx, nx_sx, &parent->mul.listen_head) {
	new->pf = sx->pf;
	new->parent = sx->xd;
	new->vf = sx->vf;
	if (new->vf->init(xd) == 0)
	    return 0;
    }
    errno = EINVAL;
    return -1;
}

static int xmul_listener_destroy(int xd);

static int xmul_listener_init(int xd) {
    struct xsock *sx = xget(xd);
    int pf = sx->pf;
    int sub_xd;
    struct xsock *sub_sx;
    struct xsock_vf *vf, *nx;

    xsock_vf_walk_safe(vf, nx, &xgb.xsock_vf_head) {
	if ((pf & vf->pf) != vf->pf)
	    continue;
	if ((sub_xd = xlisten(pf & vf->pf, sx->addr)) < 0) {
	BAD:
	    xmul_listener_destroy(xd);
	    return -1;
	}
	pf &= ~vf->pf;
	sub_sx = xget(sub_xd);
	list_add_tail(&sub_sx->link, &sx->mul.listen_head);
    }
    if (pf)
	goto BAD;
    return 0;
}

static int xmul_listener_destroy(int xd) {
    struct xsock *sx = xget(xd);
    struct xsock *sub_sx, *nx_sx;

    xsock_walk_safe(sub_sx, nx_sx, &sx->mul.listen_head) {
	list_del_init(&sub_sx->link);
	xclose(sub_sx->xd);
    }
    BUG_ON(!list_empty(&sx->mul.listen_head));
    return 0;
}






static int xmul_init(int xd) {
    struct xsock *sx = xget(xd);

    INIT_LIST_HEAD(&sx->mul.listen_head);
    switch (sx->ty) {
    case XACCEPTER:
	return xmul_accepter_init(xd);
    case XLISTENER:
	return xmul_listener_init(xd);
    }
    errno = EINVAL;
    return -1;
}



static void xmul_destroy(int xd) {

}



static struct xsock_vf ipc_and_inp_xsock_vf = {
    .pf = PF_INPROC|PF_IPC,
    .init = xmul_init,
    .destroy = xmul_destroy,
    .snd_notify = null,
    .rcv_notify = null,
};

static struct xsock_vf ipc_and_net_xsock_vf = {
    .pf = PF_NET|PF_IPC,
    .init = xmul_init,
    .destroy = xmul_destroy,
    .snd_notify = null,
    .rcv_notify = null,
};

static struct xsock_vf net_and_inp_xsock_vf = {
    .pf = PF_NET|PF_INPROC,
    .init = xmul_init,
    .destroy = xmul_destroy,
    .snd_notify = null,
    .rcv_notify = null,
};

static struct xsock_vf ipc_inp_net_xsock_vf = {
    .pf = PF_NET|PF_INPROC|PF_IPC,
    .init = xmul_init,
    .destroy = xmul_destroy,
    .snd_notify = null,
    .rcv_notify = null,
};

struct xsock_vf *ipc_and_inp_xsock_vfptr = &ipc_and_inp_xsock_vf;
struct xsock_vf *net_and_inp_xsock_vfptr = &net_and_inp_xsock_vf;
struct xsock_vf *ipc_and_net_xsock_vfptr = &ipc_and_net_xsock_vf;
struct xsock_vf *ipc_inp_net_xsock_vfptr = &ipc_inp_net_xsock_vf;
