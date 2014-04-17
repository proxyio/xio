#include "xbase.h"


static int xmul_accepter_init(int xd) {
    return 0;
}

static int xmul_connector_init(int xd) {
    return 0;
}

static int xmul_listener_init(int xd) {
    return 0;
}


static int xmul_init(int xd) {
    struct xsock *xs = xget(xd);
    
    switch (xs->ty) {
    case XACCEPTER:
	return xmul_accepter_init(xd);
    case XCONNECTOR:
	return xmul_connector_init(xd);
    case XLISTENER:
	return xmul_listener_init(xd);
    }
    errno = EINVAL;
    return -1;
}



static void xmul_destroy(int xd) {

}



static struct xsock_vf mul_xsock_vf = {
    .pf = PF_INPROC,
    .init = xmul_init,
    .destroy = xmul_destroy,
    .snd_notify = null,
    .rcv_notify = null,
};

struct xsock_vf *mul_xsock_vfptr = &mul_xsock_vf;
