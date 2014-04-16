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
    struct ep_msg s;
    struct ep_hdr *h;
    struct ep_rt *r;
    struct fd *f;
    u32 hdr_and_rt_len = sizeof(*h) + sizeof(*r);

    s.payload = xallocmsg(xmsglen(req) + hdr_and_rt_len);
    if (!s.payload) {
	return -1;
    }
    /* Append ep_msg header and route. The proxy package frame header */
    h = s.h = (struct ep_hdr *)s.payload;
    h->version = 0;
    h->ttl = 1;
    h->end_ttl = 0;
    h->go = true;
    h->size = xmsglen(req);
    h->timeout = 0;
    h->checksum = 0;
    h->sendstamp = rt_mstime();

    memcpy(s.payload + sizeof(*h), req, xmsglen(req));
    xfreemsg(req);
    r = s.r = (struct ep_rt *)(s.payload + sizeof(*h) + h->size);

    /* Update header checksum */
    ep_msg_gensum(&s);

    /* RoundRobin algo select a struct fd */
    BUG_ON(!(f = rtb_rrbin_go(&y->tb)));
    uuid_copy(r->uuid, f->st.ud);
    rc = xsend(f->cd, s.payload);
    DEBUG_OFF("channel %d send req into network", f->cd);
    return rc;
}

int ep_recv_resp(struct ep *ep, char **resp) {
    struct fd *f;
    int n;
    char *payload;
    struct ep_msg s;
    struct ep_hdr *h;
    struct pxy *y = &ep->y;
    struct xpoll_event ev = {};

    if ((n = xpoll_wait(y->po, &ev, 1, 0x7fff)) < 0)
	return -1;
    DEBUG_ON("%s", xpoll_str[ev.happened]);
    BUG_ON(ev.happened & XPOLLOUT);
    if (!(ev.happened & XPOLLIN))
	goto AGAIN;
    f = (struct fd *)ev.self;
    if (xrecv(f->cd, &payload) == 0) {
	DEBUG_ON("channel %d recv resp", f->cd);
	ep_msg_init(&s, payload);

	/* Drop the timeout message */
	if (ep_msg_timeout(&s) < 0) {
	    DEBUG_ON("message is timeout");
	    xfreemsg(payload);
	    goto AGAIN;
	}

	/* If message has invalid header checkusm. return error */
	if (ep_msg_validate(&s) < 0) {
	    DEBUG_ON("invalid message's checksum");
	    xfreemsg(payload);
	    f->fok = false;
	    goto AGAIN;
	}
	h = s.h;
	if (!(*resp = xallocmsg(h->size))) {
	    xfreemsg(payload);
	    goto AGAIN;
	}
	/* Copy response into user-space */
	memcpy(*resp, payload + sizeof(*h), h->size);

	/* Payload was copy into user-space. */
	xfreemsg(payload);

	DEBUG_ON("channel %d send req into network", f->cd);
	return 0;
    } else if (errno != EAGAIN) {
	DEBUG_ON("channel %d on bad status", f->cd);
	f->fok = false;
    }
    /* TODO: cleanup the bad status fd here */
 AGAIN:
    errno = EAGAIN;
    return -1;
}

/* Comsumer endpoint api : recv_req and send_resp */
int ep_recv_req(struct ep *ep, char **req, char **r) {
    struct fd *f;
    int n;
    char *payload;
    struct ep_msg s;
    struct ep_hdr *h;
    struct pxy *y = &ep->y;
    struct xpoll_event ev = {};

    if ((n = xpoll_wait(y->po, &ev, 1, 0x7fff)) < 0)
	return -1;
    DEBUG_ON("%s", xpoll_str[ev.happened]);
    BUG_ON(ev.happened & XPOLLOUT);
    if (!(ev.happened & XPOLLIN))
	goto AGAIN;

    f = (struct fd *)ev.self;
    if (xrecv(f->cd, &payload) == 0) {
	ep_msg_init(&s, payload);

	/* Drop the timeout message */
	if (ep_msg_timeout(&s) < 0) {
	    DEBUG_ON("message is timeout");
	    xfreemsg(payload);
	    goto AGAIN;
	}
	/* If message has invalid header checkusm. return error */
	if (ep_msg_validate(&s) < 0) {
	    DEBUG_ON("invalid message's checksum");
	    xfreemsg(payload);
	    f->fok = false;
	    goto AGAIN;
	}
	h = s.h;
	if (!(*req = xallocmsg(h->size))) {
	    xfreemsg(payload);
	    goto AGAIN;
	}
	if (!(*r = xallocmsg(sizeof(*h) + rt_size(h)))) {
	    xfreemsg(payload);
	    xfreemsg(*req);
	    goto AGAIN;
	}
	/* Copy req into user-space */
	memcpy(*req, payload + sizeof(*h), h->size);
	memcpy(*r, h, sizeof(*h));
	memcpy((*r) + sizeof(*h), s.r, rt_size(h));

	/* Payload was copy into user-space. */
	xfreemsg(payload);

	DEBUG_ON("channel %d recv req from network", f->cd);
	return 0;
    } else if (errno != EAGAIN) {
	DEBUG_ON("channel %d on bad status", f->cd);
	f->fok = false;
    }
    /* TODO: cleanup the bad status fd here */
 AGAIN:
    errno = EAGAIN;
    return -1;
}

int ep_send_resp(struct ep *ep, char *resp, char *r) {
    int rc;
    struct ep_msg s;
    struct ep_hdr *h = (struct ep_hdr *)r;
    struct fd *f;
    struct ep_rt *cr;
    struct pxy *y = &ep->y;

    s.payload = xallocmsg(xmsglen(resp) + xmsglen(r));
    if (!s.payload)
	return -1;

    /* Copy header */
    h->end_ttl = h->ttl;
    h->go = false;
    h->size = xmsglen(resp);
    memcpy(s.payload, (char *)h, sizeof(*h));
    h = s.h = (struct ep_hdr *)s.payload;

    /* Copy response payload */
    memcpy(s.payload + sizeof(*h), resp, h->size);

    /* Copy route */
    s.r = (struct ep_rt *)(s.payload + sizeof(*h) + h->size);
    memcpy(s.r, r + sizeof(*h), rt_size(h));

    xfreemsg(r);
    xfreemsg(resp);

    /* Update header checksum */
    ep_msg_gensum(&s);

    cr = rt_cur(&s);
    BUG_ON(!(f = rtb_route_back(&y->tb, cr->uuid)));
    DEBUG_OFF("channel %d send resp into network", f->cd);
    rc = xsend(f->cd, s.payload);
    return rc;
}

