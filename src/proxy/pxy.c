/* Open for DEBUGGING */
#define TRACE_DEBUG
#include <os/alloc.h>
#include <channel/channel.h>
#include <os/timesz.h>
#include "pxy.h"

/* URL example : 
 * net    group@net://182.33.49.10
 * ipc    group@ipc://xxxxxxxxxxxxxxxxxx
 * inproc group@inproc://xxxxxxxxxxxxxxxxxxxx
 */
int url_parse_group(const char *url, char *buff, u32 size) {
    char *at = strchr(url, '@');;

    if (!at) {
	errno = EINVAL;
	return -1;
    }
    strncpy(buff, url, size <= at - url ? size : at - url);
    return 0;
}

int url_parse_pf(const char *url) {
    char *at = strchr(url, '@');;
    char *pf = strstr(url, "://");;

    if (!pf || at >= pf) {
	errno = EINVAL;
	return -1;
    }
    if (at)
	++at;
    else
	at = (char *)url;
    if (strncmp(at, "net", pf - at) == 0)
	return PF_NET;
    else if (strncmp(at, "ipc", pf - at) == 0)
	return PF_IPC;
    else if (strncmp(at, "inproc", pf - at) == 0)
	return PF_INPROC;
    errno = EINVAL;
    return -1;
}

int url_parse_sockaddr(const char *url, char *buff, u32 size) {
    char *tok = "://";
    char *sock = strstr(url, tok);

    if (!sock) {
	errno = EINVAL;
	return -1;
    }
    sock += strlen(tok);
    strncpy(buff, sock, size);
    return 0;
}


struct fd *fd_new() {
    struct fd *f = (struct fd *)mem_zalloc(sizeof(*f));
    if (f) {
	f->ok = true;
	f->cd = -1;
	INIT_LIST_HEAD(&f->mq);
	INIT_LIST_HEAD(&f->link);
	f->rb_link.key = (char *)f->uuid;
	f->rb_link.keylen = sizeof(uuid_t);
    }
    return f;
}

void fd_free(struct fd *f) {
    if (f->cd >= 0)
	channel_close(f->cd);
    mem_free(f, sizeof(*f));
}

struct xg *xg_new() {
    struct xg *g = (struct xg *)mem_zalloc(sizeof(*g));
    if (g) {
	g->ref = 0;
	ssmap_init(&g->fdmap);
	g->pxy_rb_link.key = g->group;
	INIT_LIST_HEAD(&g->rcver_head);
	INIT_LIST_HEAD(&g->snder_head);
	INIT_LIST_HEAD(&g->link);
    }
    return g;
}

void xg_free(struct xg *g) {
    struct fd *f, *nf;

    list_for_each_fd(f, nf, &g->rcver_head) {
	xg_rm(g, f);
	fd_free(f);
    }
    list_for_each_fd(f, nf, &g->snder_head) {
	xg_rm(g, f);
	fd_free(f);
    }
    mem_free(g, sizeof(*g));
}

#define PXY_LOOPSTOP 1

struct pxy *pxy_new() {
    struct pxy *y = (struct pxy *)mem_zalloc(sizeof(*y));
    if (y) {
	spin_init(&y->lock);
	y->flags = PXY_LOOPSTOP;
	y->tb = upoll_create();
	y->gsz = 0;
	INIT_LIST_HEAD(&y->g_head);
	ssmap_init(&y->gmap);
	INIT_LIST_HEAD(&y->listener_head);
	INIT_LIST_HEAD(&y->unknown_head);
    }
    return y;
}

int pxy_put(struct pxy *y, struct xg *g);

void pxy_free(struct pxy *y) {
    struct xg *g, *ng;
    struct fd *f, *nf;
    
    if (!y) {
	errno = EINVAL;
	return;
    }
    spin_destroy(&y->lock);
    upoll_close(y->tb);
    list_for_each_xg(g, ng, &y->g_head) {
	pxy_put(y, g);
    }
    list_for_each_fd(f, nf, &y->listener_head) {
	list_del_init(&f->link);
	fd_free(f);
    }
    list_for_each_fd(f, nf, &y->unknown_head) {
	list_del_init(&f->link);
	fd_free(f);
    }
    mem_free(y, sizeof(*y));
}


struct fd *xg_find(struct xg *g, uuid_t ud) {
    ssmap_node_t *node;

    if ((node = ssmap_find(&g->fdmap, (char *)ud, sizeof(uuid_t))))
	return cont_of(node, struct fd, rb_link);
    return 0;
}

int xg_add(struct xg *g, struct fd *f) {
    struct fd *of = xg_find(g, f->uuid);

    if (of) {
	errno = EEXIST;
	return -1;
    }
    switch (f->ty) {
    case RECEIVER:
	g->rsz++;
	list_add(&f->link, &g->rcver_head);
	break;
    case DISPATCHER:
	g->ssz++;
	list_add(&f->link, &g->snder_head);
	break;
    default:
	BUG_ON(1);
    }
    ssmap_insert(&g->fdmap, &f->rb_link);
    return 0;
}

int xg_rm(struct xg *g, struct fd *f) {
    switch (f->ty) {
    case RECEIVER:
	g->rsz--;
	break;
    case DISPATCHER:
	g->rsz--;
	break;
    default:
	BUG_ON(1);
    }
    list_del_init(&f->link);
    ssmap_delete(&g->fdmap, &f->rb_link);
    return 0;
}

struct fd *xg_rrbin_go(struct xg *g) {
    struct fd *f;

    if (g->ssz <= 0)
	return 0;
    f = list_first(&g->snder_head, struct fd, link);
    list_move_tail(&f->link, &g->snder_head);
    return f;
}

struct fd *xg_route_back(struct xg *g, uuid_t ud) {
    return xg_find(g, ud);
}

static int try_disable_eventout(struct fd *f) {
    int rc;
    struct pxy *y = f->y;

    if (f->event.care & UPOLLOUT) {
	DEBUG_ON("disable %d UPOLLOUT", f->cd);
	f->event.care &= ~UPOLLOUT;
	BUG_ON((rc = upoll_ctl(y->tb, UPOLL_MOD, &f->event)) != 0);
	return rc;
    }
    return -1;
}

static int try_enable_eventout(struct fd *f) {
    int rc;
    struct pxy *y = f->y;

    if (!(f->event.care & UPOLLOUT)) {
	DEBUG_ON("enable %d UPOLLOUT", f->cd);
	f->event.care |= UPOLLOUT;
	BUG_ON((rc = upoll_ctl(y->tb, UPOLL_MOD, &f->event)) != 0);
	return rc;
    }
    return -1;
}

static struct gsm *mq_pop(struct fd *f) {
    struct gsm *s = 0;

    if (!list_empty(&f->mq)) {
	f->mq_size--;
	s = list_first(&f->mq, struct gsm, link);
	list_del_init(&s->link);
    } else if (list_empty(&f->mq))
	try_disable_eventout(f);
    return s;
}

static int mq_push(struct fd *f, struct gsm *s) {
    f->mq_size++;
    list_add_tail(&s->link, &f->mq);
    if (!list_empty(&f->mq))
	try_enable_eventout(f);
    return 0;
}

struct xg *pxy_get(struct pxy *y, char *group) {
    ssmap_node_t *node;
    struct xg *g;

    if ((node = ssmap_find(&y->gmap, group, strlen(group)))) {
	g = cont_of(node, struct xg, pxy_rb_link);
	g->ref++;
	return g;
    }
    if (!(g = xg_new()))
	return 0;
    g->ref++;
    list_add(&y->g_head, &g->link);

    /* Map[key] */
    strcpy(g->group, group);
    g->pxy_rb_link.keylen = strlen(group);
    ssmap_insert(&y->gmap, &g->pxy_rb_link);
    return g;
}

int pxy_put(struct pxy *y, struct xg *g) {
    g->ref--;
    if (g->ref == 0) {
	list_del_init(&g->link);
	ssmap_delete(&y->gmap, &g->pxy_rb_link);
	xg_free(g);
    }
    return 0;
}




/* Receive one request from frontend channel */
static void rcver_recv(struct fd *f) {
    i64 now = rt_mstime();
    struct fd *gof;
    struct gsm *s;
    char *payload;
    struct xg *g = f->g;

    while (channel_recv(f->cd, &payload) == 0) {
	/* TODO: should we lazzy drop this message if no any dispatchers ? */
	if (g->ssz <= 0 || !(s = gsm_new(payload))) {
	    channel_freemsg(payload);
	    DEBUG_ON("no any dispatchers");
	    continue;
	}
	/* Drop the timeout massage */
	if (gsm_timeout(s, now) < 0) {
	    gsm_free(s);
	    DEBUG_ON("message is timeout");
	    continue;
	}
	/* If massage has invalid checksum. set fd in bad status */
	if (gsm_validate(s) < 0) {
	    gsm_free(s);
	    f->ok = false;
	    DEBUG_ON("invalid message's checksum");
	    break;
	}
	tr_go_cost(s, now);

	/* Round robin algo, selete a gof */
	BUG_ON((gof = xg_rrbin_go(g)) == 0);
	mq_push(gof, s);

	DEBUG_ON("%d recv one req and push into %d", f->cd, gof->cd);
    }

    /* EPIPE */
    f->ok = (errno == EAGAIN) ? true : false;
}

/* Send one response to frontend channel */
static void rcver_send(struct fd *f) {
    i64 now = rt_mstime();
    struct gsm *s;

    if (!(s = mq_pop(f)))
	return;
    DEBUG_ON("%d pop one resp and send into network", f->cd);

    tr_shrink_and_back(s, now);
    if (channel_send(f->cd, s->payload) < 0) {
	f->ok = (errno == EAGAIN) ? true : false;
	goto EXIT;
    }

    /* payload was send into network. */
    s->payload = 0;
 EXIT:
    gsm_free(s);
}


/* Dispatch one request to backend channel */
static void snder_recv(struct fd *f) {
    i64 now = rt_mstime();
    struct fd *backf;
    struct gsm *s;
    struct tr *r;
    char *payload;
    struct xg *g = f->g;

    while (channel_recv(f->cd, &payload) == 0) {
	if (g->rsz <= 0 || !(s = gsm_new(payload))) {
	    channel_freemsg(payload);
	    continue;
	}
	/* Drop the timeout massage */
	if (gsm_timeout(s, now) < 0) {
	    gsm_free(s);
	    continue;
	}
	/* If massage has invalid checksum. set fd in bad status */
	if (gsm_validate(s) < 0) {
	    gsm_free(s);
	    f->ok = false;
	    break;
	}
	tr_back_cost(s, now);
	r = tr_prev(s);
	/* TODO: if not found any backfd. drop this response */
	BUG_ON(!(backf = xg_route_back(g, r->uuid)));
	mq_push(backf, s);
    }

    /* EPIPE */
    f->ok = (errno == EAGAIN) ? true : false;
}


/* Receive one response from backend channel */
static void snder_send(struct fd *f) {
    i64 now = rt_mstime();
    struct gsm *s;
    struct tr r = {};
    
    if (!(s = mq_pop(f)))
	return;
    uuid_copy(r.uuid, f->uuid);
    if (tr_append_and_go(s, &r, now) < 0) {
	DEBUG_ON("error on appending route chunk");
	goto EXIT;
    }
    DEBUG_ON("%d pop req and send into network", f->cd);
    if (channel_send(f->cd, s->payload) < 0) {
	if (errno != EAGAIN)
	    f->ok = false;
	goto EXIT;
    }
    /* payload was send into network. */
    s->payload = 0;

 EXIT:
    gsm_free(s);
}


static void rcver_event_handler(struct fd *f, u32 events) {
    DEBUG_ON("%d receiver has events %u", f->cd, events);
    if (f->ok && (events & UPOLLIN))
	rcver_recv(f);
    if (f->ok && (events & UPOLLOUT))
	rcver_send(f);
}

static void snder_event_handler(struct fd *f, u32 events) {
    DEBUG_ON("%d dispatcher has events %u", f->cd, events);
    if (f->ok && (events & UPOLLIN))
	snder_recv(f);
    if (f->ok && (events & UPOLLOUT))
	snder_send(f);
}

static void pxy_connector_rgs(struct fd *f, u32 events) {
    struct hgr *h;
    struct pxy *y = f->y;

    if (!(events & UPOLLIN))
	return;
    if (channel_recv(f->cd, (char **)&h) == 0) {
	/* If the unregister channel's first message invalid, set bad status */
	if (channel_msglen((char *)h) != sizeof(*h) || !(h->type & (PRODUCER|COMSUMER))) {
	    channel_freemsg((char *)h);
	    f->ok = false;
	    return;
	}

	/* Detach from pxy's unknown_head */
	list_del_init(&f->link);
	uuid_copy(f->uuid, h->id);
	f->ty = h->type;
	BUG_ON(!(f->g = pxy_get(y, h->group)));
	xg_add(f->g, f);
	DEBUG_ON("register an %s", (h->type == PRODUCER) ? "RECEIVER" : "COMSUMER");
    } else if (errno != EAGAIN) {
	f->ok = false;
    }
}

static void pxy_connector_handler(struct fd *f, u32 events) {
    if (!f->ty)
	/* Siteup channel */
	pxy_connector_rgs(f, events);

    switch (f->ty) {
    case RECEIVER:
	rcver_event_handler(f, events);
	break;
    case DISPATCHER:
	snder_event_handler(f, events);
	break;
    }

    /* If fd status bad. destroy it */
    if (!f->ok) {
	/* do something here */
    }
}

static void pxy_listener_handler(struct fd *f, u32 events) {
    struct pxy *y = f->y;
    int on = 1;
    int new_cd;
    struct fd *newf;
    
    BUG_ON(events & UPOLLOUT);
    if (events & UPOLLIN) {
	while ((new_cd = channel_accept(f->cd)) >= 0) {
	    if (!(newf = fd_new())) {
		channel_close(new_cd);
		break;
	    }
	    /* NOBLOCKING */
	    channel_setopt(new_cd, CHANNEL_NOBLOCK, &on, sizeof(on));
	    newf->h = pxy_connector_handler;
	    newf->y = y;
	    newf->event.cd = new_cd;
	    newf->event.care = UPOLLIN|UPOLLOUT|UPOLLERR;
	    newf->event.self = newf;
	    newf->cd = new_cd;
	    list_add_tail(&newf->link, &y->unknown_head);
	    BUG_ON(upoll_ctl(y->tb, UPOLL_ADD, &newf->event) != 0);
	}
    }

    /* If listener fd status bad. destroy it and we should relisten */
    if (!f->ok) {
	/* do something here */
    }
}


int pxy_listen(struct pxy *y, const char *url) {
    struct fd *f = fd_new();
    int on = 1;
    int pf = url_parse_pf(url);
    char sockaddr[URLNAME_MAX] = {};

    if (!f)
	return -1;
    if (pf < 0 || url_parse_sockaddr(url, sockaddr, URLNAME_MAX) < 0) {
	fd_free(f);
	errno = EINVAL;
	return -1;
    }
    if ((f->cd = channel_listen(pf, sockaddr)) < 0) {
	fd_free(f);
	return -1;
    }
    /* NOBLOCKING */
    channel_setopt(f->cd, CHANNEL_NOBLOCK, &on, sizeof(on));
    list_add(&f->link, &y->listener_head);
    f->event.cd = f->cd;
    f->event.care = UPOLLIN|UPOLLERR;
    f->event.self = f;
    f->y = y;
    f->h = pxy_listener_handler;
    BUG_ON(upoll_ctl(y->tb, UPOLL_ADD, &f->event) != 0);
    return 0;
}


/* Export this api for ep.c. the most code are the same */
int __pxy_connect(struct pxy *y, int ty, u32 ev, const char *url) {
    struct fd *f = fd_new();
    struct hgr *h;
    int on = 1;
    int pf = url_parse_pf(url);
    char sockaddr[URLNAME_MAX] = {}, group[URLNAME_MAX] = {};
    
    if (!f)
	return -1;
    if (pf < 0 || url_parse_group(url, group, URLNAME_MAX) < 0
	|| url_parse_sockaddr(url, sockaddr, URLNAME_MAX) < 0) {
	fd_free(f);
	errno = EINVAL;
	return -1;
    }
    if (!(h = (struct hgr *)channel_allocmsg(sizeof(*h)))) {
	fd_free(f);
	return -1;
    }
    if ((f->cd = channel_connect(pf, sockaddr)) < 0) {
    EXIT:
	fd_free(f);
	channel_freemsg((char *)h);
	return -1;
    }
    /* NOBLOCKING */
    channel_setopt(f->cd, CHANNEL_NOBLOCK, &on, sizeof(on));

    /* Generate register header for gofd */
    strcpy(h->group, group);
    uuid_generate(f->uuid);
    uuid_copy(h->id, f->uuid);
    h->type = ty;
    if (channel_send(f->cd, (char *)h) < 0)
	goto EXIT;

    f->ty = (h->type == PRODUCER) ? DISPATCHER : RECEIVER;
    f->event.cd = f->cd;
    f->event.care = ev;
    f->event.self = f;
    f->h = pxy_connector_handler;
    f->y = y;
    BUG_ON(!(f->g = pxy_get(y, h->group)));
    xg_add(f->g, f);
    BUG_ON(upoll_ctl(y->tb, UPOLL_ADD, &f->event) != 0);
    return 0;
}


int pxy_connect(struct pxy *y, const char *url) {
    return __pxy_connect(y, PRODUCER, UPOLLIN|UPOLLOUT|UPOLLERR, url);
}

static int pxy_stopped(struct pxy *y) {
    int stopped;
    spin_lock(&y->lock);
    stopped = y->flags & PXY_LOOPSTOP;
    spin_unlock(&y->lock);
    return stopped;
}

int pxy_onceloop(struct pxy *y) {
    struct fd *f;
    int i, n;
    struct upoll_event ev[100] = {};

    /* Default io events buf is 100 and timeout is 1ms */
    if ((n = upoll_wait(y->tb, ev, 100, 10)) <= 0)
	return -1;
    for (i = 0; i < n; i++) {
	f = (struct fd *)ev[i].self;
	f->h(f, ev[i].happened);
    }
    return 0;
}

static int pxy_loop_thread(void *args) {
    struct pxy *y = (struct pxy *)args;

    while (!pxy_stopped(y))
	pxy_onceloop(y);
    return 0;
}


int pxy_startloop(struct pxy *y) {
    int rc;
    
    spin_lock(&y->lock);
    /* Backend loop thread running */
    if (!(y->flags & PXY_LOOPSTOP)) {
	spin_unlock(&y->lock);
	return -1;
    }
    y->flags &= ~PXY_LOOPSTOP;
    spin_unlock(&y->lock);
    BUG_ON((rc = thread_start(&y->backend, pxy_loop_thread, y) != 0));
    return rc;
}

void pxy_stoploop(struct pxy *y) {
    spin_lock(&y->lock);
    /* Backend loop thread already stopped */
    if (y->flags & PXY_LOOPSTOP) {
	spin_unlock(&y->lock);
	return;
    }
    y->flags |= PXY_LOOPSTOP;
    spin_unlock(&y->lock);
    thread_stop(&y->backend);
}
