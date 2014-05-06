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

static void req_ep_destroy(struct epbase *ep) {

}

static void req_ep_add(struct epbase *ep, struct epsk *sk, char *ubuf) {
    struct xmsg *msg = cont_of(ubuf, struct xmsg, vec.chunk);
    struct sphdr *h = ubuf2sphdr(ubuf);
    h->ttl--;
    mutex_lock(&ep->lock);
    list_add_tail(&msg->item, &ep->rcv.head);
    ep->rcv.buf += xmsglen(ubuf);
    BUG_ON(ep->rcv.waiters < 0);
    if (ep->rcv.waiters)
	condition_broadcast(&ep->cond);
    mutex_unlock(&ep->lock);
}

static int req_ep_rm(struct epbase *ep, struct epsk *sk, char **ubuf) {
    int rc;
    struct xmsg *msg;
    struct xcmsg ent;
    struct sphdr *h = sphdr_new();
    struct spr *r = spr_new();

    mutex_lock(&ep->lock);
    if (list_empty(&ep->snd.head)) {
	mutex_unlock(&ep->lock);
	return -1;
    }
    BUG_ON(!(msg = list_first(&ep->snd.head, struct xmsg, item)));
    list_del_init(&msg->item);
    BUG_ON(ep->snd.waiters < 0);
    if (ep->snd.waiters)
	condition_broadcast(&ep->cond);
    mutex_unlock(&ep->lock);

    *ubuf = msg->vec.chunk;
    ent.idx = 0;
    ent.outofband = (char *)h;
    BUG_ON((rc = xmsgctl(*ubuf, XMSG_SETCMSG, &ent)) != 0);
    h->go = 1;
    uuid_copy(r->uuid, sk->uuid);
    rt_append(*ubuf, r);
    return 0;
}

static int req_ep_setopt(struct epbase *ep, int opt, void *optval, int optlen) {
    return 0;
}

static int req_ep_getopt(struct epbase *ep, int opt, void *optval, int *optlen) {
    return 0;
}

struct epbase_vfptr req_epbase = {
    .sp_family = SP_REQREP,
    .sp_type = REQ,
    .alloc = req_ep_alloc,
    .destroy = req_ep_destroy,
    .add = req_ep_add,
    .rm = req_ep_rm,
    .setopt = req_ep_setopt,
    .getopt = req_ep_getopt,
};

struct epbase_vfptr *req_epbase_vfptr = &req_epbase;

