/*
  Copyright (c) 2013-2014 Dong Fang. All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#include "sp_module.h"

struct sp_global sg;


static void epsk_bad_status(struct epsk *sk) {
    struct epbase *ep = sk->owner;
    mutex_lock(&ep->lock);
    list_move_tail(&sk->item, &ep->bad_socks);
    mutex_unlock(&ep->lock);
}

void sg_add_sk(struct epsk *sk) {
    int rc;
    spin_lock(&sg.lock);
    rc = xpoll_ctl(sg.po, XPOLL_ADD, &sk->ent);
    spin_unlock(&sg.lock);
    BUG_ON(rc);
}

void sg_rm_sk(struct epsk *sk) {
    int rc;
    spin_lock(&sg.lock);
    rc = xpoll_ctl(sg.po, XPOLL_DEL, &sk->ent);
    spin_unlock(&sg.lock);
    BUG_ON(rc);
}

void __sg_update_sk(struct epsk *sk, u32 ev) {
    int rc;
    sk->ent.care = ev;
    rc = xpoll_ctl(sg.po, XPOLL_MOD, &sk->ent);
    BUG_ON(rc);
}

void sg_update_sk(struct epsk *sk, u32 ev) {
    spin_lock(&sg.lock);
    __sg_update_sk(sk, ev);
    spin_unlock(&sg.lock);
}

static void connector_event_hndl(struct epsk *sk) {
    int rc;
    char *ubuf = 0;
    int happened = sk->ent.happened;
    struct epbase *ep = sk->owner;

    if (happened & XPOLLIN) {
	if ((rc = xrecv(sk->fd, &ubuf)) == 0) {
	    rc = ep->vfptr.add(ep, sk, ubuf);
	} else if (errno != EAGAIN)
	    happened |= XPOLLERR;
    }
    if (happened & XPOLLOUT) {
	if ((rc = ep->vfptr.rm(ep, sk, &ubuf)) == 0) {
	    if ((rc = xsend(sk->fd, ubuf)) < 0) {
		xfreemsg(ubuf);
		if (errno != EAGAIN)
		    happened |= XPOLLERR;
	    }
	}
    }
    if (happened & XPOLLERR) {
	DEBUG_OFF("socket %d epipe", sk->fd);
	sg_rm_sk(sk);
	epsk_bad_status(sk);
    }
}

static void listener_event_hndl(struct epsk *sk) {
    int nfd;
    int happened = sk->ent.happened;
    struct epbase *ep = sk->owner;

    if (happened & XPOLLIN) {
	while ((nfd = xaccept(sk->fd)) >= 0) {
	    ep->vfptr.join(ep, sk, nfd);
	}
	if (errno != EAGAIN)
	    happened |= XPOLLERR;
    }
    if (happened & XPOLLERR) {
	DEBUG_OFF("socket %d epipe", sk->fd);
	sg_rm_sk(sk);
	epsk_bad_status(sk);
    }
}

static void event_hndl(struct xpoll_event *ent) {
    int socktype = 0;
    int optlen;
    struct epsk *sk = (struct epsk *)ent->self;

    sk->ent.happened = ent->happened;
    xgetopt(sk->fd, XL_SOCKET, XSOCKTYPE, &socktype, &optlen);
    switch (socktype) {
    case XCONNECTOR:
	connector_event_hndl(sk);
	break;
    case XLISTENER:
	listener_event_hndl(sk);
	break;
    default:
	BUG_ON(1);
    }
}

extern const char *xpoll_str[];

static int po_routine_worker(void *args) {
    int rc, i;
    const char *estr;
    struct xpoll_event ent[100];

    while (!sg.exiting) {
	rc = xpoll_wait(sg.po, ent, NELEM(ent, struct xpoll_event), 1);
	if (rc < 0)
	    continue;
	DEBUG_OFF("%d sockets happened events", rc);
	for (i = 0; i < rc; i++) {
	    estr = xpoll_str[ent[i].happened];
	    DEBUG_OFF("socket %d with events %s", ent[i].xd, estr);
	    event_hndl(&ent[i]);
	}
    }
    return 0;
}

extern struct epbase_vfptr *req_epbase_vfptr;
extern struct epbase_vfptr *rep_epbase_vfptr;

void sp_module_init() {
    int eid;

    sg.exiting = false;
    spin_init(&sg.lock);
    sg.po = xpoll_create();
    BUG_ON(!sg.po);
    for (eid = 0; eid < XIO_MAX_ENDPOINTS; eid++) {
	sg.unused[eid] = eid;
    }
    sg.nendpoints = 0;
    thread_start(&sg.po_routine, po_routine_worker, 0);
    INIT_LIST_HEAD(&sg.epbase_head);
    list_add_tail(&req_epbase_vfptr->item, &sg.epbase_head);
    list_add_tail(&rep_epbase_vfptr->item, &sg.epbase_head);
}


void sp_module_exit() {
    sg.exiting = true;
    thread_stop(&sg.po_routine);
    spin_destroy(&sg.lock);
    xpoll_close(sg.po);
}

int eid_alloc(int sp_family, int sp_type) {
    struct epbase_vfptr *vfptr = epbase_vfptr_lookup(sp_family, sp_type);
    int eid;
    struct epbase *ep;

    if (!vfptr) {
	errno = EPROTO;
	return -1;
    }
    if (!(ep = vfptr->alloc()))
	return -1;
    spin_lock(&sg.lock);
    BUG_ON(sg.nendpoints >= XIO_MAX_ENDPOINTS);
    eid = sg.unused[sg.nendpoints++];
    sg.endpoints[eid] = ep;
    spin_unlock(&sg.lock);
    return eid;
}

struct epbase *eid_get(int eid) {
    struct epbase *ep;
    spin_lock(&sg.lock);
    if (!(ep = sg.endpoints[eid])) {
	spin_unlock(&sg.lock);
	return 0;
    }
    BUG_ON(!atomic_read(&ep->ref));
    atomic_inc(&ep->ref);
    spin_unlock(&sg.lock);
    return ep;
}

void eid_put(int eid) {
    struct epbase *ep = sg.endpoints[eid];

    BUG_ON(!ep);
    atomic_dec_and_lock(&ep->ref, spin, sg.lock) {
	sg.unused[--sg.nendpoints] = eid;
	spin_unlock(&sg.lock);
	ep->vfptr.destroy(ep);
    }
}

struct epsk *epsk_new() {
    struct epsk *sk = (struct epsk *)mem_zalloc(sizeof(*sk));
    return sk;
}

void epsk_free(struct epsk *sk) {
    mem_free(sk, sizeof(*sk));
}


void epbase_init(struct epbase *ep) {
    atomic_init(&ep->ref);
    mutex_init(&ep->lock);
    condition_init(&ep->cond);

    ep->rcv.wnd = 0;
    ep->rcv.size = 0;
    ep->rcv.waiters = 0;
    INIT_LIST_HEAD(&ep->rcv.head);

    ep->snd.wnd = 0;
    ep->snd.size = 0;
    ep->snd.waiters = 0;
    INIT_LIST_HEAD(&ep->snd.head);

    INIT_LIST_HEAD(&ep->listeners);
    INIT_LIST_HEAD(&ep->connectors);
    INIT_LIST_HEAD(&ep->bad_socks);
}

void epbase_exit(struct epbase *ep) {
    struct xmsg *msg, *nmsg;
    struct epsk *sk, *nsk;
    struct list_head closed_head;

    BUG_ON(atomic_read(&ep->ref));
    atomic_destroy(&ep->ref);
    mutex_destroy(&ep->lock);
    condition_destroy(&ep->cond);

    INIT_LIST_HEAD(&closed_head);
    list_splice(&ep->snd.head, &closed_head);
    list_splice(&ep->rcv.head, &closed_head);
    walk_msg_safe(msg, nmsg, &closed_head) {
	list_del_init(&msg->item);
	xfreemsg(msg->vec.chunk);
    }
    BUG_ON(!list_empty(&closed_head));
    INIT_LIST_HEAD(&closed_head);
    list_splice(&ep->listeners, &closed_head);
    list_splice(&ep->connectors, &closed_head);
    list_splice(&ep->bad_socks, &closed_head);

    walk_epsk_safe(sk, nsk, &closed_head) {
	list_del_init(&sk->item);
	xclose(sk->fd);
	epsk_free(sk);
    }
}
