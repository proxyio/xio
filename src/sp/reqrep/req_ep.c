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

static struct epbase *req_ep_alloc() {
    struct req_ep *req_ep = (struct req_ep *)mem_zalloc(sizeof(*req_ep));

    if (req_ep) {
        epbase_init(&req_ep->base);
        req_ep->peer = 0;
    }
    return &req_ep->base;
}

static void req_ep_destroy(struct epbase *ep)
{
    struct req_ep *req_ep = cont_of(ep, struct req_ep, base);
    BUG_ON(!req_ep);
    epbase_exit(ep);
    mem_free(req_ep, sizeof(*req_ep));
}

static int req_ep_add(struct epbase *ep, struct tgtd *tg, char *ubuf)
{
    struct xmsg *msg = cont_of(ubuf, struct xmsg, vec.xiov_base);
    struct rrhdr *rr_hdr = get_rrhdr(ubuf);

    rr_hdr->ttl--;
    DEBUG_OFF("ep %d recv resp %10.10s from socket %d", ep->eid, ubuf, tg->fd);
    mutex_lock(&ep->lock);
    list_add_tail(&msg->item, &ep->rcv.head);
    ep->rcv.size += xubuflen(ubuf);
    BUG_ON(ep->rcv.waiters < 0);
    if (ep->rcv.waiters)
        condition_broadcast(&ep->cond);
    mutex_unlock(&ep->lock);
    return 0;
}

static int req_ep_send(struct epbase *ep, char *ubuf)
{
    int rc = -1;
    struct rrhdr *rr_hdr = 0;
    struct rrr rt = {};
    struct tgtd *tg = 0;
    struct xcmsg ent = {};

    mutex_lock(&ep->lock);
    get_tgtd_if(tg, &ep->connectors, 1);
    if (tg)
        list_move_tail(&tg->item, &ep->connectors);
    mutex_unlock(&ep->lock);
    uuid_copy(rt.uuid, tg->uuid);
    rr_hdr = rqhdr_first(&rt);
    ent.outofband = (char *)rr_hdr;
    BUG_ON((rc = xmsgctl(ubuf, XMSG_ADDCMSG, &ent)));
    DEBUG_OFF("ep %d send req %10.10s to socket %d", ep->eid, ubuf, tg->fd);
    rc = xsend(tg->fd, ubuf);
    return rc;
}

static int req_ep_rm(struct epbase *ep, struct tgtd *tg, char **ubuf)
{
    int rc = -1;
    return rc;
}

static int req_ep_term(struct epbase *ep, struct tgtd *tg, int fd)
{
    int rc = tg ? sp_generic_term_by_tgtd(ep, tg) : sp_generic_term_by_fd(ep, fd);
    return rc;
}

static int req_ep_join(struct epbase *ep, struct tgtd *tg, int fd)
{
    struct tgtd *_tg = sp_generic_join(ep, fd);

    if (!_tg)
        return -1;
    uuid_generate(_tg->uuid);
    return 0;
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

static int req_ep_setopt(struct epbase *ep, int opt, void *optval, int optlen)
{
    int rc;
    if (opt < 0 || opt >= NELEM(setopt_vfptr, ep_setopt) || !setopt_vfptr[opt]) {
        errno = EINVAL;
        return -1;
    }
    rc = setopt_vfptr[opt] (ep, optval, optlen);
    return rc;
}

static int req_ep_getopt(struct epbase *ep, int opt, void *optval, int *optlen)
{
    int rc;
    if (opt < 0 || opt >= NELEM(getopt_vfptr, ep_getopt) || !getopt_vfptr[opt]) {
        errno = EINVAL;
        return -1;
    }
    rc = getopt_vfptr[opt] (ep, optval, optlen);
    return rc;
}

struct epbase_vfptr req_epbase = {
    .sp_family = SP_REQREP,
    .sp_type = SP_REQ,
    .alloc = req_ep_alloc,
    .destroy = req_ep_destroy,
    .send = req_ep_send,
    .add = req_ep_add,
    .rm = req_ep_rm,
    .join = req_ep_join,
    .term = req_ep_term,
    .setopt = req_ep_setopt,
    .getopt = req_ep_getopt,
};

struct epbase_vfptr *req_epbase_vfptr = &req_epbase;

