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

#include "rep_ep.h"

static struct epbase *rep_ep_alloc() {
    struct rep_ep *rep_ep = (struct rep_ep *)mem_zalloc(sizeof(*rep_ep));

    if (rep_ep) {
	epbase_init(&rep_ep->base);
	rep_ep->peer = 0;
    }
    return &rep_ep->base;
}

static void rep_ep_destroy(struct epbase *ep) {
    struct rep_ep *rep_ep = cont_of(ep, struct rep_ep, base);
    BUG_ON(!rep_ep);
    epbase_exit(ep);
}

static int rep_ep_add(struct epbase *ep, struct epsk *sk, char *ubuf) {
    struct xmsg *msg = cont_of(ubuf, struct xmsg, vec.chunk);
    struct spr *r = rt_cur(ubuf);
    
    if (memcmp(r->uuid, sk->uuid, sizeof(sk->uuid)) != 0) {
	uuid_copy(sk->uuid, r->uuid);
    }
    mutex_lock(&ep->lock);
    list_add_tail(&msg->item, &ep->rcv.head);
    ep->rcv.size += xmsglen(ubuf);
    BUG_ON(ep->rcv.waiters < 0);
    if (ep->rcv.waiters)
	condition_broadcast(&ep->cond);
    mutex_unlock(&ep->lock);
    return 0;
}

static void __routeback(struct epbase *ep, struct xmsg *msg) {
    struct epsk *sk, *nsk;
    char *ubuf = msg->vec.chunk;
    struct spr *rt = rt_cur(ubuf);

    walk_epsk_safe(sk, nsk, &ep->connectors) {
	if (memcmp(sk->uuid, rt->uuid, sizeof(sk->uuid)) != 0)
	    continue;
	list_add_tail(&msg->item, &sk->snd_cache);
	return;
    }
    xfreemsg(ubuf);
}

static void routeback(struct epbase *ep) {
    struct xmsg *msg, *nmsg;

    mutex_lock(&ep->lock);
    walk_msg_safe(msg, nmsg, &ep->snd.head) {
	list_del_init(&msg->item);
	__routeback(ep, msg);
    }
    mutex_unlock(&ep->lock);
}

static int rep_ep_rm(struct epbase *ep, struct epsk *sk, char **ubuf) {
    struct xmsg *msg;
    struct sphdr *h;
    int cmsgnum = 0;
    
    DEBUG_OFF("begin");
    routeback(ep);
    if (list_empty(&sk->snd_cache))
	return -1;
    BUG_ON(!(msg = list_first(&sk->snd_cache, struct xmsg, item)));
    list_del_init(&msg->item);
    *ubuf = msg->vec.chunk;
    
    mutex_lock(&ep->lock);
    ep->snd.size -= xmsglen(*ubuf);
    BUG_ON(ep->snd.waiters < 0);
    if (ep->snd.waiters)
	condition_broadcast(&ep->cond);
    mutex_unlock(&ep->lock);

    BUG_ON(xmsgctl(*ubuf, XMSG_CMSGNUM, &cmsgnum));
    BUG_ON(!cmsgnum);
    h = ubuf2sphdr(*ubuf);
    h->go = 0;
    h->end_ttl = h->ttl;
    DEBUG_OFF("ok");
    return 0;
}

static int rep_ep_join(struct epbase *ep, struct epsk *sk, int nfd) {
    struct epsk *nsk = sp_generic_join(ep, nfd);

    if (!nsk)
	return -1;
    return 0;
}

static int set_pipeline(struct epbase *ep, void *optval, int optlen) {
    int rc, backend_eid = *(int *)optval;
    struct epbase *peer = eid_get(backend_eid);

    if (!peer) {
	errno = EBADF;
	return -1;
    }
    rc = epbase_pipeline(ep, peer);
    eid_put(backend_eid);
    return rc;
}

static const ep_setopt setopt_vfptr[] = {
    0,
    set_pipeline,
};

static const ep_getopt getopt_vfptr[] = {
    0,
    0,
};

static int rep_ep_setopt(struct epbase *ep, int opt, void *optval, int optlen) {
    int rc;
    if (opt < 0 || opt >= NELEM(setopt_vfptr, ep_setopt) || !setopt_vfptr[opt]) {
	errno = EINVAL;
	return -1;
    }
    rc = setopt_vfptr[opt] (ep, optval, optlen);
    return rc;
}

static int rep_ep_getopt(struct epbase *ep, int opt, void *optval, int *optlen) {
    int rc;
    if (opt < 0 || opt >= NELEM(getopt_vfptr, ep_getopt) || !getopt_vfptr[opt]) {
	errno = EINVAL;
	return -1;
    }
    rc = getopt_vfptr[opt] (ep, optval, optlen);
    return rc;
}

struct epbase_vfptr rep_epbase = {
    .sp_family = SP_REQREP,
    .sp_type = SP_REP,
    .alloc = rep_ep_alloc,
    .destroy = rep_ep_destroy,
    .add = rep_ep_add,
    .rm = rep_ep_rm,
    .join = rep_ep_join,
    .setopt = rep_ep_setopt,
    .getopt = rep_ep_getopt,
};

struct epbase_vfptr *rep_epbase_vfptr = &rep_epbase;






