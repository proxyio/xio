#include <os/alloc.h>
#include <x/xsock.h>
#include <os/timesz.h>
#include <hash/crc.h>
#include "pxy.h"
#include "ep.h"

#define DEFAULT_GROUP "default"


struct ep {
    int type;
    struct pxy y;
};


struct ep *ep_new(int ty) {
    struct ep *ep = (struct ep *)mem_zalloc(sizeof(*ep));
    if (ep) {
	pxy_init(&ep->y);
	ep->type = ty;
    }
    return ep;
}

void ep_close(struct ep *ep) {
    pxy_destroy(&ep->y);
    mem_free(ep, sizeof(*ep));
}

extern int __pxy_connect(struct pxy *y, int ty, u32 ev, const char *url);

int ep_connect(struct ep *ep, const char *url) {
    return __pxy_connect(&ep->y, ep->type, XPOLLERR|XPOLLIN, url);
}

/* Producer endpoint api : send_req and recv_resp */
int ep_send_req(struct ep *ep, char *req) {
    int rc;
    struct pxy *y = &ep->y;
    struct ep_hdr *h;
    struct ep_rt *cr;
    struct fd *f;
    u32 hdr_sz = sizeof(*h) + sizeof(*cr);

    if (!(h = (struct ep_hdr *)xallocmsg(xmsglen(req) + hdr_sz)))
	return -1;

    /* Append ep_hdr and route. The proxy package frame header */
    h->version = 0;
    h->ttl = 1;
    h->end_ttl = 0;
    h->go = true;
    h->size = xmsglen(req);
    h->timeout = 0;
    h->checksum = 0;
    h->sendstamp = rt_mstime();

    memcpy((char *)h + hdr_size(h), req, xmsglen(req));
    xfreemsg(req);

    /* Update header checksum */
    ep_hdr_gensum(h);

    /* RoundRobin algo select a struct fd */
    BUG_ON(!(f = rtb_rrbin_go(&y->tb)));
    cr = rt_cur(h);
    uuid_copy(cr->uuid, f->st.ud);
    rc = xsend(f->xd, (char *)h);
    DEBUG_OFF("channel %d send req into network", f->xd);
    return rc;
}

int ep_recv_resp(struct ep *ep, char **resp) {
    int n;
    struct pxy *y = &ep->y;
    struct fd *f;
    struct ep_hdr *h;
    struct xpoll_event ev = {};

    if ((n = xpoll_wait(y->po, &ev, 1, 0x7fff)) < 0)
	return -1;
    DEBUG_ON("%s", xpoll_str[ev.happened]);
    BUG_ON(ev.happened & XPOLLOUT);
    if (!(ev.happened & XPOLLIN))
	goto AGAIN;
    f = (struct fd *)ev.self;
    if (xrecv(f->xd, (char **)&h) == 0) {
	DEBUG_ON("channel %d recv resp", f->xd);

	/* Drop the timeout message */
	if (ep_hdr_timeout(h) < 0) {
	    DEBUG_ON("message is timeout");
	    xfreemsg((char *)h);
	    goto AGAIN;
	}

	/* If message has invalid header checkusm. return error */
	if (ep_hdr_validate(h) < 0) {
	    DEBUG_ON("invalid message's checksum");
	    xfreemsg((char *)h);
	    f->fok = false;
	    goto AGAIN;
	}
	if (!(*resp = xallocmsg(h->size))) {
	    xfreemsg((char *)h);
	    goto AGAIN;
	}

	/* Copy response into user-space */
	memcpy(*resp, (char *)h + hdr_size(h), h->size);

	/* Payload was copy into user-space. */
	xfreemsg((char *)h);

	DEBUG_ON("channel %d send req into network", f->xd);
	return 0;
    } else if (errno != EAGAIN) {
	DEBUG_ON("channel %d on bad status", f->xd);
	f->fok = false;
    }
    /* TODO: cleanup the bad status fd here */
 AGAIN:
    errno = EAGAIN;
    return -1;
}

/* Comsumer endpoint api : recv_req and send_resp */
int ep_recv_req(struct ep *ep, char **req, char **r) {
    int n;
    struct pxy *y = &ep->y;
    struct fd *f;
    struct ep_hdr *h;
    struct xpoll_event ev = {};

    if ((n = xpoll_wait(y->po, &ev, 1, 0x7fff)) < 0)
	return -1;
    DEBUG_ON("%s", xpoll_str[ev.happened]);
    BUG_ON(ev.happened & XPOLLOUT);
    if (!(ev.happened & XPOLLIN))
	goto AGAIN;

    f = (struct fd *)ev.self;
    if (xrecv(f->xd, (char **)&h) == 0) {
	/* Drop the timeout message */
	if (ep_hdr_timeout(h) < 0) {
	    DEBUG_ON("message is timeout");
	    xfreemsg((char *)h);
	    goto AGAIN;
	}
	/* If message has invalid header checkusm. return error */
	if (ep_hdr_validate(h) < 0) {
	    DEBUG_ON("invalid message's checksum");
	    xfreemsg((char *)h);
	    f->fok = false;
	    goto AGAIN;
	}
	if (!(*req = xallocmsg(h->size))) {
	    xfreemsg((char *)h);
	    goto AGAIN;
	}
	if (!(*r = xallocmsg(hdr_size(h)))) {
	    xfreemsg((char *)h);
	    xfreemsg(*req);
	    goto AGAIN;
	}
	/* Copy req into user-space */
	memcpy(*req, (char *)h + hdr_size(h), h->size);
	memcpy(*r, h, hdr_size(h));

	/* Payload was copy into user-space. */
	xfreemsg((char *)h);

	DEBUG_ON("channel %d recv req from network", f->xd);
	return 0;
    } else if (errno != EAGAIN) {
	DEBUG_ON("channel %d on bad status", f->xd);
	f->fok = false;
    }
    /* TODO: cleanup the bad status fd here */
 AGAIN:
    errno = EAGAIN;
    return -1;
}

int ep_send_resp(struct ep *ep, char *resp, char *r) {
    int rc;
    struct pxy *y = &ep->y;
    struct fd *f;
    struct ep_hdr *h = (struct ep_hdr *)r;
    struct ep_hdr *nh;
    struct ep_rt *cr;

    if (!(nh = (struct ep_hdr *)xallocmsg(xmsglen(resp) + xmsglen(r))))
	return -1;
    memcpy(nh, h, hdr_size(h));
    memcpy((char *)nh + hdr_size(h), resp, xmsglen(resp));

    /* Copy header */
    nh->end_ttl = nh->ttl;
    nh->go = false;
    nh->size = xmsglen(resp);

    xfreemsg(r);
    xfreemsg(resp);

    /* Update header checksum */
    ep_hdr_gensum(nh);
    cr = rt_cur(nh);
    BUG_ON(!(f = rtb_route_back(&y->tb, cr->uuid)));
    DEBUG_OFF("channel %d send resp into network", f->xd);
    rc = xsend(f->xd, (char *)nh);
    return rc;
}

