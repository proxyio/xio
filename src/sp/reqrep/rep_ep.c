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

}

static int rep_ep_add(struct epbase *ep, struct epsk *sk, char *ubuf) {
    struct spr *r = rt_cur(ubuf);

    if (memcmp(r->uuid, sk->uuid, sizeof(sk->uuid)) != 0)
	uuid_copy(sk->uuid, r->uuid);
    return 0;
}

static int rep_ep_rm(struct epbase *ep, struct epsk *sk, char **ubuf) {
    struct xmsg *msg;
    struct sphdr *h;

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
    h = ubuf2sphdr(*ubuf);
    h->go = 0;
    h->end_ttl = h->ttl;
    return 0;
}

static void rep_ep_join(struct epbase *ep, struct epsk *sk, int nfd) {
    struct epsk *nsk = epsk_new();

    if (!nsk) {
	xclose(nfd);
	return;
    }
    nsk->owner = ep;
    nsk->ent.xd = nfd;
    nsk->ent.self = nsk;
    nsk->ent.care = XPOLLIN|XPOLLOUT|XPOLLERR;
    sg_add_sk(nsk);
}

static int rep_ep_setopt(struct epbase *ep, int opt, void *optval, int optlen) {
    return 0;
}

static int rep_ep_getopt(struct epbase *ep, int opt, void *optval, int *optlen) {
    return 0;
}

struct epbase_vfptr rep_epbase = {
    .sp_family = SP_REQREP,
    .sp_type = REQ,
    .alloc = rep_ep_alloc,
    .destroy = rep_ep_destroy,
    .add = rep_ep_add,
    .rm = rep_ep_rm,
    .join = rep_ep_join,
    .setopt = rep_ep_setopt,
    .getopt = rep_ep_getopt,
};

struct epbase_vfptr *rep_epbase_vfptr = &rep_epbase;






