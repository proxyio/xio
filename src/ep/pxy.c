#include <os/alloc.h>
#include <channel/channel_base.h>
#include <os/timesz.h>
#include "pxy.h"

extern struct channel *cid_to_channel(int cd);

const char *ep_str[] = {
    "RECEIVER",
    "DISPATCHER",
};

void pxy_init(struct pxy *y) {
    mutex_init(&y->mtx);
    y->fstopped = true;
    rtb_init(&y->tb);
    y->po = upoll_create();
    INIT_LIST_HEAD(&y->listener);
    INIT_LIST_HEAD(&y->unknown);
}



void pxy_destroy(struct pxy *y) {
    struct fd *f, *nf;

    BUG_ON(!y->fstopped);
    mutex_destroy(&y->mtx);
    rtb_destroy(&y->tb);
    upoll_close(y->po);
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

    if (f->event.care & UPOLLOUT) {
	DEBUG_OFF("disable %d UPOLLOUT", f->cd);
	f->event.care &= ~UPOLLOUT;
	BUG_ON((rc = upoll_ctl(y->po, UPOLL_MOD, &f->event)) != 0);
    }
}

static void try_enable_eventout(struct fd *f) {
    int rc = 0;
    struct pxy *y = fd_getself(f, struct pxy);

    if (!(f->event.care & UPOLLOUT)) {
	DEBUG_OFF("enable %d UPOLLOUT", f->cd);
	f->event.care |= UPOLLOUT;
	BUG_ON((rc = upoll_ctl(y->po, UPOLL_MOD, &f->event)) != 0);
    }
}

static struct ep_msg *mq_pop(struct fd *f) {
    struct ep_msg *s = 0;

    if (!list_empty(&f->mq)) {
	f->mq_size--;
	s = list_first(&f->mq, struct ep_msg, link);
	list_del_init(&s->link);
    } else if (list_empty(&f->mq))
	try_disable_eventout(f);
    return s;
}

static int mq_push(struct fd *f, struct ep_msg *s) {
    f->mq_size++;
    list_add_tail(&s->link, &f->mq);
    if (!list_empty(&f->mq))
	try_enable_eventout(f);
    return 0;
}

/* Receive one request from frontend channel */
static void rcver_recv(struct fd *f) {
    struct fd *gof;
    struct ep_msg *s;
    char *payload;
    struct xg *g = f->g;

    while (channel_recv(f->cd, &payload) == 0) {
	/* TODO: should we lazzy drop this message if no any dispatchers ? */
	if (g->ssz <= 0 || !(s = ep_msg_new(payload))) {
	    DEBUG_OFF("no any dispatchers");
	    channel_freemsg(payload);
	    continue;
	}
	/* Drop the timeout massage */
	if (ep_msg_timeout(s) < 0) {
	    DEBUG_OFF("message is timeout");
	    ep_msg_free(s);
	    continue;
	}
	/* If massage has invalid checksum. set fd in bad status */
	if (ep_msg_validate(s) < 0) {
	    DEBUG_OFF("invalid message's checksum");
	    ep_msg_free(s);
	    f->fok = false;
	    break;
	}
	rt_go_cost(s);

	/* Round robin algo, selete a gof */
	BUG_ON((gof = xg_rrbin_go(g)) == 0);
	mq_push(gof, s);

	DEBUG_OFF("%d recv req and push into %d", f->cd, gof->cd);
    }

    /* EPIPE */
    if (!(f->fok = (errno == EAGAIN) ? true : false)) {
	DEBUG_OFF("EPIPE");
    }
}

/* Send one response to frontend channel */
static void rcver_send(struct fd *f) {
    struct ep_msg *s;

    if (!(s = mq_pop(f)))
	return;
    DEBUG_OFF("%d pop resp and send into network", f->cd);

    rt_shrink_and_back(s);
    if (channel_send(f->cd, s->payload) < 0) {
	f->fok = (errno == EAGAIN) ? true : false;
	goto EXIT;
    }

    /* payload was send into network. */
    s->payload = 0;
 EXIT:
    ep_msg_free(s);
}


/* Dispatch one request to backend channel */
static void snder_recv(struct fd *f) {
    struct fd *backf;
    struct ep_msg *s;
    struct ep_rt *r;
    char *payload;
    struct xg *g = f->g;

    while (channel_recv(f->cd, &payload) == 0) {
	if (g->rsz <= 0 || !(s = ep_msg_new(payload))) {
	    DEBUG_OFF("no any receivers");
	    channel_freemsg(payload);
	    continue;
	}
	/* Drop the timeout massage */
	if (ep_msg_timeout(s) < 0) {
	    DEBUG_OFF("message is timeout");
	    ep_msg_free(s);
	    continue;
	}
	/* If massage has invalid checksum. set fd in bad status */
	if (ep_msg_validate(s) < 0) {
	    DEBUG_OFF("invalid message's checksum");
	    ep_msg_free(s);
	    f->fok = false;
	    break;
	}
	rt_back_cost(s);
	r = rt_prev(s);
	/* TODO: if not found any backfd. drop this response */
	BUG_ON(!(backf = xg_route_back(g, r->uuid)));
	mq_push(backf, s);

	DEBUG_OFF("%d recv resp and push into %d", f->cd, backf->cd);
    }

    /* EPIPE */
    if (!(f->fok = (errno == EAGAIN) ? true : false))
	DEBUG_OFF("EPIPE");
}


/* Receive one response from backend channel */
static void snder_send(struct fd *f) {
    struct ep_msg *s;
    struct ep_rt r = {};
    
    if (!(s = mq_pop(f)))
	return;
    
    uuid_copy(r.uuid, f->st.ud);
    if (rt_append_and_go(s, &r) < 0) {
	DEBUG_OFF("error on appending route chunk");
	goto EXIT;
    }
    if (channel_send(f->cd, s->payload) < 0) {
	if (errno != EAGAIN)
	    f->fok = false;
	goto EXIT;
    }
    DEBUG_OFF("%d pop req and send into network", f->cd);

    /* payload was send into network. */
    s->payload = 0;

 EXIT:
    ep_msg_free(s);
}


static void rcver_event_handler(struct fd *f, u32 events) {
    struct channel *cn = cid_to_channel(f->cd);

    DEBUG_OFF("%d receiver has events %s and recv-Q:%ld send-Q:%ld",
	      f->cd, upoll_str[events], cn->rcv, cn->snd);
    if (f->fok && (events & UPOLLIN))
	rcver_recv(f);
    if (f->fok && (events & UPOLLOUT))
	rcver_send(f);
    if (f->fok && (events & UPOLLERR)) {
	f->fok = false;
	DEBUG_OFF("%d bad status", f->cd);
    }
}

static void snder_event_handler(struct fd *f, u32 events) {
    struct channel *cn = cid_to_channel(f->cd);

    DEBUG_OFF("%d dispatcher has events %s and recv-Q:%ld send-Q:%ld",
	      f->cd, upoll_str[events], cn->rcv, cn->snd);
    if (f->fok && (events & UPOLLIN))
	snder_recv(f);
    if (f->fok && (events & UPOLLOUT))
	snder_send(f);
    if (f->fok && (events & UPOLLERR)) {
	f->fok = false;
	DEBUG_OFF("%d bad status", f->cd);
    }
}

static void pxy_connector_rgs(struct fd *f, u32 events) {
    struct ep_stat *syn;
    struct pxy *y = fd_getself(f, struct pxy);

    if (!(events & UPOLLIN))
	return;
    if (channel_recv(f->cd, (char **)&syn) == 0) {
	/* Recv syn */
	if (channel_msglen((char *)syn) != sizeof(f->st)
	    || !(syn->type & (RECEIVER|DISPATCHER))) {
	    channel_freemsg((char *)syn);
	    f->fok = false;
	    DEBUG_OFF("recv invalid syn from channel %d", f->cd);
	    return;
	}
	DEBUG_OFF("recv syn from channel %d", f->cd);

	/* Detach from pxy's unknown_head */
	list_del_init(&f->link);

	memcpy(&f->st, syn, sizeof(f->st));
	BUG_ON(rtb_mapfd(&y->tb, f));

	/* Send synack */
	BUG_ON(channel_send(f->cd, (char *)syn) != 0);
	DEBUG_OFF("send syn to channel %d", f->cd);
	DEBUG_ON("pxy register an %s", ep_str[f->st.type]);
    } else if (errno != EAGAIN) {
	DEBUG_ON("unregister channel %d EPIPE", f->cd);
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
	DEBUG_OFF("%s channel %d EPIPE", ep_str[f->st.type], f->cd);
	rtb_unmapfd(&y->tb, f);
	BUG_ON(upoll_ctl(y->po, UPOLL_DEL, &f->event) != 0);
	fd_free(f);
    }
}

static void pxy_listener_handler(struct fd *f, u32 events) {
    struct pxy *y = fd_getself(f, struct pxy);
    int on = 1;
    int new_cd;
    struct fd *newf;
    
    DEBUG_ON("listener events %s", upoll_str[events]);
    if (events & UPOLLIN) {
	while ((new_cd = channel_accept(f->cd)) >= 0) {
	    if (!(newf = fd_new())) {
		channel_close(new_cd);
		break;
	    }
	    /* NOBLOCKING */
	    channel_setopt(new_cd, CHANNEL_NOBLOCK, &on, sizeof(on));
	    newf->self = y;
	    newf->h = pxy_connector_handler;
	    newf->event.cd = new_cd;
	    newf->event.care = UPOLLIN|UPOLLOUT|UPOLLERR;
	    newf->event.self = newf;
	    newf->cd = new_cd;
	    list_add_tail(&newf->link, &y->unknown);
	    BUG_ON(upoll_ctl(y->po, UPOLL_ADD, &newf->event) != 0);
	    DEBUG_ON("listener create a new channel %d", new_cd);
	}
    }

    /* If listener fd status bad. destroy it and we should relisten */
    if (!f->fok || (events & UPOLLERR)) {
	DEBUG_OFF("listener endpoint %d EPIPE", f->cd);
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
    list_add(&f->link, &y->listener);
    f->self = y;
    f->event.cd = f->cd;
    f->event.care = UPOLLIN|UPOLLERR;
    f->event.self = f;
    f->h = pxy_listener_handler;
    BUG_ON(upoll_ctl(y->po, UPOLL_ADD, &f->event) != 0);
    DEBUG_ON("channel %d listen on sockaddr %s", f->cd, url);
    return 0;
}


/* Export this api for ep.c. the most code are the same */
int __pxy_connect(struct pxy *y, int ty, u32 ev, const char *url) {
    struct fd *f = fd_new();
    struct ep_stat *syn;
    int on = 1;
    int pf = url_parse_pf(url);
    char sockaddr[URLNAME_MAX] = {};
    
    if (!f)
	return -1;
    /* Generate register header for gofd */
    uuid_generate(f->st.ud);
    f->st.type = ty;
    if (pf < 0 || url_parse_group(url, f->st.group, URLNAME_MAX) < 0
	|| url_parse_sockaddr(url, sockaddr, URLNAME_MAX) < 0) {
	fd_free(f);
	errno = EINVAL;
	return -1;
    }
    if (!(syn = (struct ep_stat *)channel_allocmsg(sizeof(*syn)))) {
	fd_free(f);
	return -1;
    }
    if ((f->cd = channel_connect(pf, sockaddr)) < 0) {
    EXIT:
	fd_free(f);
	channel_freemsg((char *)syn);
	return -1;
    }
    DEBUG_ON("channel %d connect ok", f->cd);
    *syn = f->st;
    syn->type = (~syn->type) & (DISPATCHER|RECEIVER);

    /* syn-send state */
    if (channel_send(f->cd, (char *)syn) < 0)
	goto EXIT;

    /* syn-ack state */
    if (channel_recv(f->cd, (char **)&syn) < 0) {
	return -1;
    }

    /* NOBLOCKING */
    channel_setopt(f->cd, CHANNEL_NOBLOCK, &on, sizeof(on));
    f->self = y;
    f->event.cd = f->cd;
    f->event.care = ev;
    f->event.self = f;
    f->h = pxy_connector_handler;
    rtb_mapfd(&y->tb, f);
    BUG_ON(upoll_ctl(y->po, UPOLL_ADD, &f->event) != 0);

    /* Free syn-ack */
    channel_freemsg((char *)syn);
    return 0;
}


int pxy_connect(struct pxy *y, const char *url) {
    return __pxy_connect(y, DISPATCHER, UPOLLIN|UPOLLOUT|UPOLLERR, url);
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
    struct upoll_event ev[100] = {};

    /* Default io events buf is 100 and timeout is 1ms */
    if ((n = upoll_wait(y->po, ev, 100, 1)) <= 0) {
	DEBUG_ON("upoll wait with no events and errno %d", errno);
	return -1;
    }
    for (i = 0; i < n; i++) {
	f = (struct fd *)ev[i].self;
	DEBUG_ON("%d %s", f->cd, upoll_str[ev[i].happened]);
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
