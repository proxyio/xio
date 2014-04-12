#include <os/alloc.h>
#include <channel/channel.h>
#include <os/timesz.h>
#include "pxy.h"
#include "ep.h"

#define DEFAULT_GROUP "default"

struct ep {
    struct hgr h;
    struct xg *g;
    struct pxy *y;
};

extern struct fd *xg_rrbin_go(struct xg *g);
extern struct xg *pxy_get(struct pxy *y, char *group);
extern int pxy_put(struct pxy *y, struct xg *g);


struct ep *ep_new(int ty) {
    struct ep *ep = (struct ep *)mem_zalloc(sizeof(*ep));
    if (ep) {
	if (!(ep->y = pxy_new())) {
	    mem_free(ep, sizeof(*ep));
	    return 0;
	}
	uuid_generate(ep->h.id);
	ep->h.type = ty;
	strcpy(ep->h.group, DEFAULT_GROUP);
	ep->g = pxy_get(ep->y, DEFAULT_GROUP);
    }
    return ep;
}

void ep_close(struct ep *ep) {
    pxy_put(ep->y, ep->g);
    pxy_free(ep->y);
    mem_free(ep, sizeof(*ep));
}

extern int __pxy_connect(struct pxy *y, int ty, u32 ev, const char *url);

int ep_connect(struct ep *ep, const char *url) {
    return __pxy_connect(ep->y, ep->h.type, UPOLLERR|UPOLLIN, url);
}

/* Producer endpoint api : send_req and recv_resp */
int ep_send_req(struct ep *ep, char *req) {
    int rc;
    struct gsm *s;
    struct rdh *h;
    struct tr *r;
    struct fd *f;

    if ((ep->h.type & COMSUMER)) {
	errno = EINVAL;
	return -1;
    }
    if (!(s = gsm_new(0)))
	return -1;
    if (!(s->payload = channel_allocmsg(channel_msglen(req) + sizeof(*s->h)
        + sizeof(*s->r)))) {
	gsm_free(s);
	return -1;
    }
    /* Append gsm header and route */
    h = s->h = (struct rdh *)s->payload;
    r = s->r = (struct tr *)(s->payload + sizeof(*h) + channel_msglen(req));

    /* The proxy package frame header */
    h->version = 0;
    h->ttl = 0;
    h->end_ttl = 0;
    h->go = true;
    h->icmp = false;
    h->size = channel_msglen(req);
    h->timeout = 0;
    h->checksum = 0;
    h->seqid = 0;
    h->sendstamp = rt_mstime();

    /* Update header checksum */
    gsm_gensum(s);

    /* RoundRobin algo select a struct fd */
    BUG_ON(!(f = xg_rrbin_go(ep->g)));
    uuid_copy(r->uuid, f->uuid);
    if ((rc = channel_send(f->cd, s->payload)) < 0) {
	gsm_free(s);
	return rc;
    }
    channel_freemsg(req);
    return 0;
}

int ep_recv_resp(struct ep *ep, char **resp) {
    struct fd *f;
    int i, n;
    char *payload;
    i64 now = rt_mstime();
    struct gsm *s;
    struct rdh *h;
    struct pxy *y = ep->y;
    struct upoll_event ev[100] = {};

    if ((ep->h.type & PRODUCER)) {
	errno = EINVAL;
	return -1;
    }
    /* Default io events buf is 100 and timeout is 0ms. NOBLOCK */
    if ((n = upoll_wait(y->tb, ev, 100, 0)) < 0)
	return -1;
    for (i = 0; i < n; i++) {
	if (!(ev[i].happened & UPOLLIN))
	    continue;
	f = (struct fd *)ev[i].self;
	if (channel_recv(f->cd, &payload) == 0) {
	    if (!(s = gsm_new(payload))) {
		channel_freemsg(payload);
		continue;
	    }
	    /* Drop the timeout message */
	    if (gsm_timeout(s, now) < 0) {
		gsm_free(s);
		continue;
	    }
	    /* If message has invalid header checkusm. return error */
	    if (gsm_validate(s) < 0) {
		gsm_free(s);
		/* Set fd bad status */
		f->ok = false;
		continue;
	    }
	    h = s->h;
	    if (!(*resp = channel_allocmsg(h->size))) {
		gsm_free(s);
		continue;
	    }
	    /* Copy response into user-space */
	    memcpy(*resp, payload + sizeof(*h), h->size);
	    return 0;
	}
    }
    errno = EAGAIN;
    return -1;
}

/* Comsumer endpoint api : recv_req and send_resp */
int ep_recv_req(struct ep *ep, char **req, char **r) {
    if ((ep->h.type & PRODUCER)) {
	errno = EINVAL;
	return -1;
    }
    return 0;
}

int ep_send_resp(struct ep *ep, char *resp, char *r) {
    if ((ep->h.type & PRODUCER)) {
	errno = EINVAL;
	return -1;
    }
    return 0;
}


char *ep_allocmsg(int size) {
    return channel_allocmsg(size);
}

void ep_freemsg(char *payload) {
    channel_freemsg(payload);
}
