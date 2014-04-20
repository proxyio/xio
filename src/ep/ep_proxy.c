#include <os/alloc.h>
#include <x/xsock.h>
#include <os/timesz.h>
#include "ep_proxy.h"

extern struct channel *xget(int xd);

const char *ep_str[] = {
    "RECEIVER",
    "DISPATCHER",
};

void pxy_init(struct pxy *y) {
    mutex_init(&y->mtx);
    y->fstopped = true;
    rtb_init(&y->tb);
    y->po = xpoll_create();
    INIT_LIST_HEAD(&y->listener);
    INIT_LIST_HEAD(&y->unknown);
}



void pxy_destroy(struct pxy *y) {
    struct fd *f, *nf;

    BUG_ON(!y->fstopped);
    mutex_destroy(&y->mtx);
    rtb_destroy(&y->tb);
    xpoll_close(y->po);
    list_for_each_fd(f, nf, &y->listener) {
	list_del_init(&f->link);
	fd_free(f);
    }
    list_for_each_fd(f, nf, &y->unknown) {
	list_del_init(&f->link);
	fd_free(f);
    }
}

static void try_disable_eventout(struct fd *f) {
    int rc = 0;
    struct pxy *y = fd_getself(f, struct pxy);

    if (f->event.care & XPOLLOUT) {
	DEBUG_OFF("disable %d XPOLLOUT", f->xd);
	f->event.care &= ~XPOLLOUT;
	BUG_ON((rc = xpoll_ctl(y->po, XPOLL_MOD, &f->event)) != 0);
    }
}

static void try_enable_eventout(struct fd *f) {
    int rc = 0;
    struct pxy *y = fd_getself(f, struct pxy);

    if (!(f->event.care & XPOLLOUT)) {
	DEBUG_OFF("enable %d XPOLLOUT", f->xd);
	f->event.care |= XPOLLOUT;
	BUG_ON((rc = xpoll_ctl(y->po, XPOLL_MOD, &f->event)) != 0);
    }
}

static struct ep_hdr *mq_pop(struct fd *f) {
    struct ep_hdr *h = 0;

    if (!list_empty(&f->mq)) {
	f->mq_size--;
	h = list_first(&f->mq, struct ep_hdr, u.link);
	list_del_init(&h->u.link);
    } else if (list_empty(&f->mq))
	try_disable_eventout(f);
    return h;
}

static int mq_push(struct fd *f, struct ep_hdr *h) {
    f->mq_size++;
    list_add_tail(&h->u.link, &f->mq);
    if (!list_empty(&f->mq))
	try_enable_eventout(f);
    return 0;
}

/* Receive one request from frontend channel */
static void rcver_recv(struct fd *f) {
    int rc;
    struct fd *gof;
    struct ep_hdr *h;
    struct xg *g = f->g;

    while ((rc = ep_recv(f->xd, &h)) == 0) {
	/* TODO: should we lazzy drop this message if no any dispatchers ? */
	if (g->ssz <= 0) {
	    DEBUG_OFF("no any dispatchers");
	    xfreemsg((char *)h);
	    continue;
	}
	rt_go_cost(h);

	/* Round robin algo, selete a gof */
	BUG_ON((gof = xg_rrbin_go(g)) == 0);
	mq_push(gof, h);
	DEBUG_OFF("%d recv req and push into %d", f->xd, gof->xd);
    }

    /* EPIPE */
    if (!(f->fok = (errno == EAGAIN) ? true : false)) {
	DEBUG_OFF("EPIPE");
    }
}

/* Send one response to frontend channel */
static void rcver_send(struct fd *f) {
    struct ep_hdr *h;

    while (f->fok && (h = mq_pop(f))) {
	rt_shrink_and_back(h);
	if (xsend(f->xd, (char *)h) < 0) {
	    xfreemsg((char *)h);
	    f->fok = (errno == EAGAIN) ? true : false;
	}
	DEBUG_OFF("%d pop resp and send into network", f->xd);
    }
}


/* Dispatch one request to backend channel */
static void snder_recv(struct fd *f) {
    struct fd *backf;
    struct ep_hdr *h;
    struct ep_rt *cr;
    struct xg *g = f->g;

    while (ep_recv(f->xd, &h) == 0) {
	if (g->rsz <= 0) {
	    DEBUG_OFF("no any receivers");
	    xfreemsg((char *)h);
	    continue;
	}
	rt_back_cost(h);
	cr = rt_prev(h);

	/* TODO: if not found any backfd. drop this response */
	BUG_ON(!(backf = xg_route_back(g, cr->uuid)));
	mq_push(backf, h);

	DEBUG_OFF("%d recv resp and push into %d", f->xd, backf->xd);
    }

    /* EPIPE */
    if (!(f->fok = (errno == EAGAIN) ? true : false))
	DEBUG_OFF("EPIPE");
}


/* Receive one response from backend channel */
static void snder_send(struct fd *f) {
    struct ep_hdr *h, *nh;
    struct ep_rt r = {};
    
    while (f->fok && (h = mq_pop(f))) {
	uuid_copy(r.uuid, f->st.ud);
	BUG_ON(!(nh = rt_append_and_go(h, &r)));
	if (xsend(f->xd, (char *)nh) < 0) {
	    xfreemsg((char *)nh);
	    f->fok = (errno == EAGAIN) ? true : false;
	}
	DEBUG_OFF("%d pop req and send into network", f->xd);
    }
}


static void rcver_event_handler(struct fd *f, u32 events) {
    struct channel *cn = xget(f->xd);

    DEBUG_OFF("%d receiver has events %s and recv-Q:%ld send-Q:%ld",
	      f->xd, xpoll_str[events], cn->rcv, cn->snd);
    if (f->fok && (events & XPOLLIN))
	rcver_recv(f);
    if (f->fok && (events & XPOLLOUT))
	rcver_send(f);
    if (f->fok && (events & XPOLLERR)) {
	f->fok = false;
	DEBUG_OFF("%d bad status", f->xd);
    }
}

static void snder_event_handler(struct fd *f, u32 events) {
    struct channel *cn = xget(f->xd);

    DEBUG_OFF("%d dispatcher has events %s and recv-Q:%ld send-Q:%ld",
	      f->xd, xpoll_str[events], cn->rcv, cn->snd);
    if (f->fok && (events & XPOLLIN))
	snder_recv(f);
    if (f->fok && (events & XPOLLOUT))
	snder_send(f);
    if (f->fok && (events & XPOLLERR)) {
	f->fok = false;
	DEBUG_OFF("%d bad status", f->xd);
    }
}

static void pxy_connector_rgs(struct fd *f, u32 events) {
    struct ep_stat *syn;
    struct pxy *y = fd_getself(f, struct pxy);

    if (!(events & XPOLLIN))
	return;
    if (xrecv(f->xd, (char **)&syn) == 0) {
	/* Recv syn */
	if (xmsglen((char *)syn) != sizeof(f->st)
	    || !(syn->type & (RECEIVER|DISPATCHER))) {
	    xfreemsg((char *)syn);
	    f->fok = false;
	    DEBUG_OFF("recv invalid syn from channel %d", f->xd);
	    return;
	}
	DEBUG_OFF("recv syn from channel %d", f->xd);

	/* Detach from pxy's unknown_head */
	list_del_init(&f->link);

	memcpy(&f->st, syn, sizeof(f->st));
	BUG_ON(rtb_mapfd(&y->tb, f));

	/* Send synack */
	BUG_ON(xsend(f->xd, (char *)syn) != 0);
	DEBUG_OFF("send syn to channel %d", f->xd);
	DEBUG_OFF("pxy register an %s", ep_str[f->st.type]);
    } else if (errno != EAGAIN) {
	DEBUG_OFF("unregister channel %d EPIPE", f->xd);
	f->fok = false;
    }
}

static void pxy_connector_handler(struct fd *f, u32 events) {
    struct pxy *y = fd_getself(f, struct pxy);

    if (!f->st.type)
	/* Siteup channel */
	pxy_connector_rgs(f, events);

    switch (f->st.type) {
    case RECEIVER:
	rcver_event_handler(f, events);
	break;
    case DISPATCHER:
	snder_event_handler(f, events);
	break;
    }

    /* If fd status bad. destroy it */
    if (!f->fok) {
	DEBUG_OFF("%s channel %d EPIPE", ep_str[f->st.type], f->xd);
	rtb_unmapfd(&y->tb, f);
	BUG_ON(xpoll_ctl(y->po, XPOLL_DEL, &f->event) != 0);
	fd_free(f);
    }
}

static void pxy_listener_handler(struct fd *f, u32 events) {
    int on = 1;
    struct pxy *y = fd_getself(f, struct pxy);
    struct fd *newf;

    DEBUG_OFF("listener events %s", xpoll_str[events]);
    if (events & XPOLLIN) {
	while ((newf = fd_accept(f))) {
	    xsetopt(newf->xd, XNOBLOCK, &on, sizeof(on));
	    newf->self = y;
	    newf->h = pxy_connector_handler;
	    newf->event.xd = newf->xd;
	    newf->event.care = XPOLLIN|XPOLLOUT|XPOLLERR;
	    newf->event.self = newf;
	    list_add_tail(&newf->link, &y->unknown);
	    BUG_ON(xpoll_ctl(y->po, XPOLL_ADD, &newf->event) != 0);
	    DEBUG_OFF("listener create a new channel %d", newf->xd);
	}
    }

    /* If listener fd status bad. destroy it and we should relisten */
    if (!f->fok || (events & XPOLLERR)) {
	DEBUG_OFF("listener endpoint %d EPIPE", f->xd);
    }
}


int pxy_listen(struct pxy *y, const char *url) {
    int on = 1;
    struct fd *f = fd_listen(url);

    if (!f)
	return -1;
    DEBUG_OFF("channel %d listen on %s", f->xd, url);

    /* NOBLOCKING */
    xsetopt(f->xd, XNOBLOCK, &on, sizeof(on));
    list_add(&f->link, &y->listener);
    f->self = y;
    f->event.xd = f->xd;
    f->event.care = XPOLLIN|XPOLLERR;
    f->event.self = f;
    f->h = pxy_listener_handler;
    BUG_ON(xpoll_ctl(y->po, XPOLL_ADD, &f->event) != 0);
    return 0;
}


/* Export this api for ep.c. the most code are the same */
int __pxy_connect(struct pxy *y, int ty, u32 ev, const char *url) {
    struct fd *f = fd_open(url);
    struct ep_stat *syn;
    int on = 1;

    if (!f)
	return -1;
    DEBUG_OFF("channel %d connect ok", f->xd);

    /* Generate register header for gofd */
    if (!(syn = (struct ep_stat *)xallocmsg(sizeof(*syn)))) {
	fd_free(f);
	return -1;
    }
    uuid_generate(f->st.ud);
    f->st.type = ty;
    *syn = f->st;
    syn->type = (~syn->type) & (DISPATCHER|RECEIVER);

    /* syn-send state */
    if (xsend(f->xd, (char *)syn) < 0) {
	xfreemsg((char *)syn);
	fd_free(f);
	return -1;
    }

    /* syn-ack state */
    if (xrecv(f->xd, (char **)&syn) < 0) {
	fd_free(f);
	return -1;
    }

    /* NOBLOCKING */
    xsetopt(f->xd, XNOBLOCK, &on, sizeof(on));
    f->self = y;
    f->event.xd = f->xd;
    f->event.care = ev;
    f->event.self = f;
    f->h = pxy_connector_handler;
    rtb_mapfd(&y->tb, f);
    BUG_ON(xpoll_ctl(y->po, XPOLL_ADD, &f->event) != 0);

    /* Free syn-ack */
    xfreemsg((char *)syn);
    return 0;
}


int pxy_connect(struct pxy *y, const char *url) {
    return __pxy_connect(y, DISPATCHER, XPOLLIN|XPOLLOUT|XPOLLERR, url);
}

static int pxy_stopped(struct pxy *y) {
    int stopped;
    mutex_lock(&y->mtx);
    stopped = y->fstopped;
    mutex_unlock(&y->mtx);
    return stopped;
}

int pxy_onceloop(struct pxy *y) {
    struct fd *f;
    int i, n;
    struct xpoll_event ev[100] = {};

    /* Default io events buf is 100 and timeout is 1ms */
    if ((n = xpoll_wait(y->po, ev, 100, 1)) <= 0) {
	DEBUG_OFF("xpoll wait with no events and errno %d", errno);
	return -1;
    }
    for (i = 0; i < n; i++) {
	f = (struct fd *)ev[i].self;
	DEBUG_OFF("%d %s", f->xd, xpoll_str[ev[i].happened]);
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
    
    mutex_lock(&y->mtx);
    /* Backend loop thread running */
    if (!y->fstopped) {
	mutex_unlock(&y->mtx);
	return -1;
    }
    y->fstopped = false;
    mutex_unlock(&y->mtx);
    BUG_ON((rc = thread_start(&y->backend, pxy_loop_thread, y) != 0));
    return rc;
}

void pxy_stoploop(struct pxy *y) {
    mutex_lock(&y->mtx);
    /* Backend loop thread already stopped */
    if (y->fstopped) {
	mutex_unlock(&y->mtx);
	return;
    }
    y->fstopped = true;
    mutex_unlock(&y->mtx);
    thread_stop(&y->backend);
}
