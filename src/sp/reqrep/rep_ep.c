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
    struct rep_ep *rep_ep = TNEW(struct rep_ep);

    if (rep_ep) {
        epbase_init(&rep_ep->base);
        rep_ep->peer = 0;
    }
    return &rep_ep->base;
}

static void rep_ep_destroy(struct epbase *ep)
{
    struct rep_ep *rep_ep = cont_of(ep, struct rep_ep, base);
    BUG_ON(!rep_ep);
    epbase_exit(ep);
    mem_free(rep_ep, sizeof(*rep_ep));
}

static int rep_ep_send(struct epbase *ep, char *ubuf)
{
    int rc = -1;
    struct rrhdr *rr_hdr = get_rrhdr(ubuf);
    struct rrr *rt = rt_cur(ubuf);
    struct tgtd *tg = 0;

    mutex_lock(&ep->lock);
    get_tgtd_if(tg, &ep->connectors, !uuid_compare(tg->uuid, rt->uuid));
    if (tg)
        list_move(&tg->item, &ep->connectors);
    mutex_unlock(&ep->lock);

    rr_hdr->go = 0;
    rr_hdr->end_ttl = rr_hdr->ttl;

    if (tg) {
        rc = xsend(tg->fd, ubuf);
        DEBUG_OFF("ep %d send resp %10.10s to socket %d", ep->eid, ubuf, tg->fd);
    }
    return rc;
}

static int rep_ep_add(struct epbase *ep, struct tgtd *tg, char *ubuf)
{
    struct xmsg *msg = cont_of(ubuf, struct xmsg, vec.xiov_base);
    struct rrr *r = rt_cur(ubuf);

    if (uuid_compare(r->uuid, tg->uuid))
        uuid_copy(tg->uuid, r->uuid);
    DEBUG_OFF("ep %d recv req %10.10s from socket %d", ep->eid, ubuf, tg->fd);
    mutex_lock(&ep->lock);
    list_add_tail(&msg->item, &ep->rcv.head);
    ep->rcv.size += xubuflen(ubuf);
    BUG_ON(ep->rcv.waiters < 0);
    if (ep->rcv.waiters)
        condition_broadcast(&ep->cond);
    mutex_unlock(&ep->lock);
    return 0;
}

static int rep_ep_rm(struct epbase *ep, struct tgtd *tg, char **ubuf)
{
    int rc = -1;
    if (tg->ent.events & XPOLLOUT)
	sg_update_tg(tg, tg->ent.events & ~XPOLLOUT);	    
    return rc;
}

static int rep_ep_join(struct epbase *ep, struct tgtd *parent, int fd)
{
    struct tgtd *_tg = sp_generic_join(ep, fd);

    if (!_tg)
        return -1;
    return 0;
}

static int rep_ep_term(struct epbase *ep, struct tgtd *tg, int fd)
{
    int rc = tg ? sp_generic_term_by_tgtd(ep, tg) : sp_generic_term_by_fd(ep, fd);
    return rc;
}


static int set_proxyto(struct epbase *ep, void *optval, int optlen)
{
    int rc, backend_eid = *(int *)optval;
    struct epbase *peer = eid_get(backend_eid);

    if (!peer) {
        errno = EBADF;
        return -1;
    }
    rc = epbase_proxyto(ep, peer);
    eid_put(backend_eid);
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

static int rep_ep_setopt(struct epbase *ep, int opt, void *optval, int optlen)
{
    int rc;
    if (opt < 0 || opt >= NELEM(setopt_vfptr, ep_setopt) || !setopt_vfptr[opt]) {
        errno = EINVAL;
        return -1;
    }
    rc = setopt_vfptr[opt] (ep, optval, optlen);
    return rc;
}

static int rep_ep_getopt(struct epbase *ep, int opt, void *optval, int *optlen)
{
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
    .send = rep_ep_send,
    .add = rep_ep_add,
    .rm = rep_ep_rm,
    .join = rep_ep_join,
    .term = rep_ep_term,
    .setopt = rep_ep_setopt,
    .getopt = rep_ep_getopt,
};

struct epbase_vfptr *rep_epbase_vfptr = &rep_epbase;






