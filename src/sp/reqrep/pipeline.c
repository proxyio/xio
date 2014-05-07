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
#include "rep_ep.h"

static void double_lock(struct epbase *ep1, struct epbase *ep2) {
    mutex_lock(&ep1->lock);
    mutex_lock(&ep2->lock);
}

static void double_unlock(struct epbase *ep1, struct epbase *ep2) {
    mutex_lock(&ep2->lock);
    mutex_lock(&ep1->lock);
}

static int receiver_add(struct epbase *ep, struct epsk *sk, char *ubuf) {
    struct xmsg *msg = cont_of(ubuf, struct xmsg, vec.chunk);
    struct spr *r = rt_cur(ubuf);
    struct epbase *peer = &(cont_of(ep, struct rep_ep, base)->peer)->base;

    if (memcmp(r->uuid, sk->uuid, sizeof(sk->uuid)) != 0) {
	uuid_copy(sk->uuid, r->uuid);
    }
    list_add_tail(&msg->item, &peer->snd.head);
    peer->snd.size += xmsglen(ubuf);
    return 0;
}

static int receiver_rm(struct epbase *ep, struct epsk *sk, char **ubuf) {
    return 0;
}

static int dispatcher_add(struct epbase *ep, struct epsk *sk, char *ubuf) {
    return 0;
}

static int dispatcher_rm(struct epbase *ep, struct epsk *sk, char **ubuf) {
    return 0;
}


int epbase_pipeline(struct epbase *rep_ep, struct epbase *req_ep) {
    struct rep_ep *frontend = cont_of(rep_ep, struct rep_ep, base);
    struct req_ep *backend = cont_of(req_ep, struct req_ep, base);

    double_lock(rep_ep, req_ep);
    if (!list_empty(&rep_ep->connectors) || !list_empty(&rep_ep->bad_socks)) {
	errno = EINVAL;
	double_unlock(rep_ep, req_ep);
	return -1;
    }
    if (!list_empty(&req_ep->connectors) || !list_empty(&req_ep->bad_socks)) {
	errno = EINVAL;
	double_unlock(rep_ep, req_ep);
	return -1;
    }
    frontend->peer = backend;
    backend->peer = frontend;
    atomic_inc(&rep_ep->ref);
    atomic_inc(&req_ep->ref);

    rep_ep->vfptr.add = receiver_add;
    rep_ep->vfptr.rm = receiver_rm;
    req_ep->vfptr.add = dispatcher_add;
    req_ep->vfptr.rm = dispatcher_rm;

    double_unlock(rep_ep, req_ep);
    return 0;
}
