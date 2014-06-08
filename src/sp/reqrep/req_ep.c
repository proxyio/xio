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

#include "req_ep.h"

static struct epbase *reqep_alloc() {
    struct reqep *reqep = TNEW(struct reqep);

    if (reqep) {
        epbase_init(&reqep->base);
        reqep->peer = 0;
	return &reqep->base;
    }
    return 0;
}

static void reqep_destroy(struct epbase *ep)
{
    struct reqep *reqep = cont_of(ep, struct reqep, base);
    BUG_ON(!reqep);
    epbase_exit(ep);
    mem_free(reqep, sizeof(*reqep));
}

static int reqep_add(struct epbase *ep, struct tgtd *tg, char *ubuf)
{
    struct rrhdr *pg = get_rrhdr(ubuf);

    if (!pg)
	return -1;
    pg->ttl--;
    DEBUG_OFF("ep %d recv resp %10.10s from socket %d", ep->eid, ubuf, tg->fd);
    mutex_lock(&ep->lock);
    skb_fifo_in(&ep->rcv, ubuf);
    BUG_ON(ep->rcv.waiters < 0);
    if (ep->rcv.waiters)
        condition_broadcast(&ep->cond);
    mutex_unlock(&ep->lock);
    return 0;
}

static int reqep_send(struct epbase *ep, char *ubuf)
{
    int rc = -1;
    struct rrhdr *pg = 0;
    struct rtentry rt = {};
    struct tgtd *tg = 0;

    mutex_lock(&ep->lock);
    get_tgtd_if(tg, &ep->connectors, 1);
    if (tg)
        list_move_tail(&tg->item, &ep->connectors);
    mutex_unlock(&ep->lock);
    uuid_copy(rt.uuid, get_rrtgtd(tg)->uuid);
    pg = new_rrhdr(&rt);
    ubufctl_add(ubuf, (char *)pg);
    DEBUG_OFF("ep %d send req %10.10s to socket %d", ep->eid, ubuf, tg->fd);
    rc = xsend(tg->fd, ubuf);
    return rc;
}

static int reqep_rm(struct epbase *ep, struct tgtd *tg, char **ubuf)
{
    int rc = -1;
    if (tg->ent.events & XPOLLOUT)
	sg_update_tg(tg, tg->ent.events & ~XPOLLOUT);	    
    return rc;
}

static void reqep_term(struct epbase *ep, struct tgtd *tg)
{
    tgtd_free(tg);
}

static struct tgtd *reqep_join(struct epbase *ep, int fd)
{
    struct rrtgtd *rr_tg = TNEW(struct rrtgtd);

    if (rr_tg) {
	skb_fifo_init(&rr_tg->snd, SP_SNDWND);
	uuid_generate(rr_tg->uuid);
	generic_tgtd_init(ep, &rr_tg->tg, fd);
    }
    return &rr_tg->tg;
}


static int set_proxyto(struct epbase *ep, void *optval, int optlen)
{
    int rc, front_eid = *(int *)optval;
    struct epbase *peer = eid_get(front_eid);

    if (!peer) {
        errno = EBADF;
        return -1;
    }
    rc = epbase_proxyto(peer, ep);
    eid_put(front_eid);
    return rc;
}


static const ep_setopt setopt_vfptr[] = {
    0,
    set_proxyto,
};

static const ep_getopt getopt_vfptr[] = {
    0,
    0,
};

static int reqep_setopt(struct epbase *ep, int opt, void *optval, int optlen)
{
    int rc;
    if (opt < 0 || opt >= NELEM(setopt_vfptr, ep_setopt) || !setopt_vfptr[opt]) {
        errno = EINVAL;
        return -1;
    }
    rc = setopt_vfptr[opt] (ep, optval, optlen);
    return rc;
}

static int reqep_getopt(struct epbase *ep, int opt, void *optval, int *optlen)
{
    int rc;
    if (opt < 0 || opt >= NELEM(getopt_vfptr, ep_getopt) || !getopt_vfptr[opt]) {
        errno = EINVAL;
        return -1;
    }
    rc = getopt_vfptr[opt] (ep, optval, optlen);
    return rc;
}

struct epbase_vfptr reqep = {
    .sp_family = SP_REQREP,
    .sp_type = SP_REQ,
    .alloc = reqep_alloc,
    .destroy = reqep_destroy,
    .send = reqep_send,
    .add = reqep_add,
    .rm = reqep_rm,
    .join = reqep_join,
    .term = reqep_term,
    .setopt = reqep_setopt,
    .getopt = reqep_getopt,
};

struct epbase_vfptr *reqep_vfptr = &reqep;

