#include <os/alloc.h>
#include <channel/channel.h>
#include <os/timesz.h>
#include <hash/crc.h>
#include "pxy.h"
#include "ep.h"

#define DEFAULT_GROUP "default"

struct ep *ep_new(int ty) {
    struct ep *ep = (struct ep *)mem_zalloc(sizeof(*ep));
    if (ep) {
	if (!(ep->y = pxy_new())) {
	    mem_free(ep, sizeof(*ep));
	    return 0;
	}
	ep->type = ty;
    }
    return ep;
}

void ep_close(struct ep *ep) {
    pxy_free(ep->y);
    mem_free(ep, sizeof(*ep));
}

extern int __pxy_connect(struct pxy *y, int ty, u32 ev, const char *url);

int ep_connect(struct ep *ep, const char *url) {
    return __pxy_connect(ep->y, ep->type, UPOLLERR|UPOLLIN, url);
}

/* Producer endpoint api : send_req and recv_resp */
int ep_send_req(struct ep *ep, char *req) {
    int rc;
    struct pxy *y = ep->y;
    struct ep_msg s;
    struct ep_hdr *h;
    struct ep_rt *r;
    struct fd *f;
    u32 hdr_and_rt_len = sizeof(*h) + sizeof(*r);

    if ((ep->type & RECEIVER)) {
	errno = EINVAL;
	return -1;
    }
    s.payload = channel_allocmsg(channel_msglen(req) + hdr_and_rt_len);
    if (!s.payload) {
	return -1;
    }
    /* Append ep_msg header and route. The proxy package frame header */
    h = s.h = (struct ep_hdr *)s.payload;
    h->version = 0;
    h->ttl = 1;
    h->end_ttl = 0;
    h->go = true;
    h->icmp = false;
    h->size = channel_msglen(req);
    h->timeout = 0;
    h->checksum = 0;
    h->seqid = 0;
    h->sendstamp = rt_mstime();

    memcpy(s.payload + sizeof(*h), req, channel_msglen(req));
    channel_freemsg(req);
    r = s.r = (struct ep_rt *)(s.payload + sizeof(*h) + h->size);

    /* Update header checksum */
    ep_msg_gensum(&s);

    /* RoundRobin algo select a struct fd */
    BUG_ON(!(f = rtb_rrbin_go(&y->tb)));
    uuid_copy(r->uuid, f->st.ud);
    rc = channel_send(f->cd, s.payload);
    DEBUG_OFF("channel %d send req into network", f->cd);
    return rc;
}

int ep_recv_resp(struct ep *ep, char **resp) {
    struct fd *f;
    int n;
    char *payload;
    struct ep_msg s;
    struct ep_hdr *h;
    struct pxy *y = ep->y;
    struct upoll_event ev = {};

    if ((ep->type & RECEIVER)) {
	errno = EINVAL;
	return -1;
    }
    /* TODO: The timeout see ep_setopt for more details */
    if ((n = upoll_wait(y->po, &ev, 1, 0x7fff)) < 0)
	return -1;
    DEBUG_ON("%s", upoll_str[ev.happened]);
    BUG_ON(ev.happened & UPOLLOUT);
    if (!(ev.happened & UPOLLIN))
	goto AGAIN;
    f = (struct fd *)ev.self;
    if (channel_recv(f->cd, &payload) == 0) {
	DEBUG_ON("channel %d recv resp", f->cd);
	ep_msg_init(&s, payload);

	/* Drop the timeout message */
	if (ep_msg_timeout(&s, rt_mstime()) < 0) {
	    DEBUG_ON("message is timeout");
	    channel_freemsg(payload);
	    goto AGAIN;
	}

	/* If message has invalid header checkusm. return error */
	if (ep_msg_validate(&s) < 0) {
	    DEBUG_ON("invalid message's checksum");
	    channel_freemsg(payload);
	    f->fok = false;
	    goto AGAIN;
	}
	h = s.h;
	if (!(*resp = channel_allocmsg(h->size))) {
	    channel_freemsg(payload);
	    goto AGAIN;
	}
	/* Copy response into user-space */
	memcpy(*resp, payload + sizeof(*h), h->size);

	/* Payload was copy into user-space. */
	channel_freemsg(payload);

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
    struct pxy *y = ep->y;
    struct upoll_event ev = {};

    if ((ep->type & DISPATCHER)) {
	errno = EINVAL;
	return -1;
    }
    /* TODO: The timeout see ep_setopt for more details */
    if ((n = upoll_wait(y->po, &ev, 1, 0x7fff)) < 0)
	return -1;
    DEBUG_ON("%s", upoll_str[ev.happened]);
    BUG_ON(ev.happened & UPOLLOUT);
    if (!(ev.happened & UPOLLIN))
	goto AGAIN;

    f = (struct fd *)ev.self;
    if (channel_recv(f->cd, &payload) == 0) {
	ep_msg_init(&s, payload);

	/* Drop the timeout message */
	if (ep_msg_timeout(&s, rt_mstime()) < 0) {
	    DEBUG_ON("message is timeout");
	    channel_freemsg(payload);
	    goto AGAIN;
	}
	/* If message has invalid header checkusm. return error */
	if (ep_msg_validate(&s) < 0) {
	    DEBUG_ON("invalid message's checksum");
	    channel_freemsg(payload);
	    f->fok = false;
	    goto AGAIN;
	}
	h = s.h;
	if (!(*req = channel_allocmsg(h->size))) {
	    channel_freemsg(payload);
	    goto AGAIN;
	}
	if (!(*r = channel_allocmsg(sizeof(*h) + rt_size(h)))) {
	    channel_freemsg(payload);
	    channel_freemsg(*req);
	    goto AGAIN;
	}
	/* Copy req into user-space */
	memcpy(*req, payload + sizeof(*h), h->size);
	memcpy(*r, h, sizeof(*h));
	memcpy((*r) + sizeof(*h), s.r, rt_size(h));

	/* Payload was copy into user-space. */
	channel_freemsg(payload);

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
    struct pxy *y = ep->y;

    if ((ep->type & DISPATCHER)
	|| channel_msglen(r) != rt_size(h) + sizeof(*h)) {
	errno = EINVAL;
	return -1;
    }
    s.payload = channel_allocmsg(channel_msglen(resp) + channel_msglen(r));
    if (!s.payload)
	return -1;

    /* Copy header */
    h->end_ttl = h->ttl;
    h->go = false;
    h->size = channel_msglen(resp);
    memcpy(s.payload, (char *)h, sizeof(*h));
    h = s.h = (struct ep_hdr *)s.payload;

    /* Copy response payload */
    memcpy(s.payload + sizeof(*h), resp, h->size);

    /* Copy route */
    s.r = (struct ep_rt *)(s.payload + sizeof(*h) + h->size);
    memcpy(s.r, r + sizeof(*h), rt_size(h));

    channel_freemsg(r);
    channel_freemsg(resp);

    /* Update header checksum */
    ep_msg_gensum(&s);

    cr = rt_cur(&s);
    BUG_ON(!(f = rtb_route_back(&y->tb, cr->uuid)));
    DEBUG_OFF("channel %d send resp into network", f->cd);
    rc = channel_send(f->cd, s.payload);
    return rc;
}


char *ep_allocmsg(int size) {
    return channel_allocmsg(size);
}

void ep_freemsg(char *payload) {
    channel_freemsg(payload);
}
