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

static void dlock(struct epbase *ep1, struct epbase *ep2)
{
    mutex_lock(&ep1->lock);
    mutex_lock(&ep2->lock);
}

static void dunlock(struct epbase *ep1, struct epbase *ep2)
{
    mutex_unlock(&ep2->lock);
    mutex_unlock(&ep1->lock);
}


static struct tgtd *rrbin_forward(struct epbase *ep, char *ubuf) {
    struct tgtd *tg = list_first(&ep->connectors, struct tgtd, item);

    list_move_tail(&tg->item, &ep->connectors);
    return tg;
}

static struct tgtd *route_backward(struct epbase *ep, char *ubuf) {
    struct rtentry *rt = rt_prev(ubuf);
    struct tgtd *tg = 0;

    get_tgtd_if(tg, &ep->connectors, !uuid_compare(get_rrtgtd(tg)->uuid, rt->uuid));
    return tg;
}

static int receiver_add(struct epbase *ep, struct tgtd *tg, char *ubuf)
{
    struct epbase *peer = &(cont_of(ep, struct repep, base)->peer)->base;
    struct skbuf *msg = cont_of(ubuf, struct skbuf, chunk.iov_base);
    struct rtentry *rt = rt_cur(ubuf);
    struct tgtd *go = rrbin_forward(peer, ubuf);

    if (uuid_compare(rt->uuid, get_rrtgtd(tg)->uuid))
        uuid_copy(get_rrtgtd(tg)->uuid, rt->uuid);
    list_add_tail(&msg->item, &get_rrtgtd(go)->sndq);
    peer->snd.size += xubuflen(ubuf);
    __tgtd_try_enable_out(go);
    DEBUG_OFF("ep %d req %10.10s from socket %d", ep->eid, ubuf, tg->fd);
    return 0;
}

static int dispatcher_rm(struct epbase *ep, struct tgtd *tg, char **ubuf)
{
    struct skbuf *msg;
    struct rtentry rt = {};

    if (list_empty(&get_rrtgtd(tg)->sndq)) {
	__tgtd_try_disable_out(tg);
	return -1;
    }
    uuid_copy(rt.uuid, get_rrtgtd(tg)->uuid);
    msg = list_first(&get_rrtgtd(tg)->sndq, struct skbuf, item);
    *ubuf = msg->chunk.iov_base;
    list_del_init(&msg->item);
    ep->snd.size -= xubuflen(*ubuf);
    rt_append(*ubuf, &rt);
    DEBUG_OFF("ep %d req %10.10s to socket %d", ep->eid, *ubuf, tg->fd);
    return 0;
}


static int dispatcher_add(struct epbase *ep, struct tgtd *tg, char *ubuf)
{
    struct epbase *peer = &(cont_of(ep, struct reqep, base)->peer)->base;
    struct skbuf *msg = cont_of(ubuf, struct skbuf, chunk.iov_base);
    struct rrhdr *pg = get_rrhdr(ubuf);
    struct tgtd *back = route_backward(peer, ubuf);

    if (!back)
        return -1;
    pg->ttl--;
    list_add_tail(&msg->item, &get_rrtgtd(back)->sndq);
    peer->snd.size += xubuflen(ubuf);
    __tgtd_try_enable_out(back);
    DEBUG_OFF("ep %d resp %10.10s from socket %d", ep->eid, ubuf, tg->fd);
    return 0;
}

static int receiver_rm(struct epbase *ep, struct tgtd *tg, char **ubuf)
{
    struct skbuf *msg = 0;

    if (list_empty(&get_rrtgtd(tg)->sndq)) {
	__tgtd_try_disable_out(tg);
        return -1;
    }
    msg = list_first(&get_rrtgtd(tg)->sndq, struct skbuf, item);
    *ubuf = msg->chunk.iov_base;
    list_del_init(&msg->item);
    ep->snd.size -= xubuflen(*ubuf);
    DEBUG_OFF("ep %d resp %10.10s to socket %d", ep->eid, *ubuf, tg->fd);
    return 0;
}

int epbase_proxyto(struct epbase *repep, struct epbase *reqep)
{
    struct repep *frontend = cont_of(repep, struct repep, base);
    struct reqep *backend = cont_of(reqep, struct reqep, base);

    dlock(repep, reqep);
    if (!list_empty(&repep->connectors) || !list_empty(&repep->bad_socks)) {
        errno = EINVAL;
        dunlock(repep, reqep);
        return -1;
    }
    if (!list_empty(&reqep->connectors) || !list_empty(&reqep->bad_socks)) {
        errno = EINVAL;
        dunlock(repep, reqep);
        return -1;
    }
    frontend->peer = backend;
    backend->peer = frontend;

    repep->vfptr.add = receiver_add;
    repep->vfptr.rm = receiver_rm;
    reqep->vfptr.add = dispatcher_add;
    reqep->vfptr.rm = dispatcher_rm;

    dunlock(repep, reqep);
    return 0;
}
