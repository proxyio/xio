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

static void dlock (struct epbase *ep1, struct epbase *ep2)
{
	mutex_lock (&ep1->lock);
	mutex_lock (&ep2->lock);
}

static void dunlock (struct epbase *ep1, struct epbase *ep2)
{
	mutex_unlock (&ep2->lock);
	mutex_unlock (&ep1->lock);
}

static struct tgtd *route_backward (struct repep *repep, char *ubuf) {
	struct epbase *ep = &repep->base;
	struct rtentry *rt = rt_prev (ubuf);
	struct tgtd *tg = 0;

	get_tgtd_if (tg, &ep->connectors, !uuid_compare (get_rr_tgtd (tg)->uuid, rt->uuid) );
	return tg;
}

static int receiver_add (struct epbase *ep, struct tgtd *tg, char *ubuf)
{
	struct reqep *peer = peer_reqep (ep);
	struct rtentry *rt = rt_cur (ubuf);
	struct tgtd *go = peer->target_algo->select (peer, ubuf);

	if (uuid_compare (rt->uuid, get_rr_tgtd (tg)->uuid) )
		uuid_copy (get_rr_tgtd (tg)->uuid, rt->uuid);
	skbuf_head_in (&get_rr_tgtd (go)->ls_head, ubuf);
	tgtd_try_enable_out (go);
	DEBUG_OFF ("ep %d req %10.10s from socket %d", ep->eid, ubuf, tg->fd);
	return 0;
}

static int dispatcher_rm (struct epbase *ep, struct tgtd *tg, char **ubuf)
{
	struct rtentry rt = {};
	if (skbuf_head_empty (&get_rr_tgtd (tg)->ls_head) ) {
		tgtd_try_disable_out (tg);
		return -1;
	}
	uuid_copy (rt.uuid, get_rr_tgtd (tg)->uuid);
	skbuf_head_out (&get_rr_tgtd (tg)->ls_head, *ubuf);
	rt_append (*ubuf, &rt);
	DEBUG_OFF ("ep %d req %10.10s to socket %d", ep->eid, *ubuf, tg->fd);
	return 0;
}


static int dispatcher_add (struct epbase *ep, struct tgtd *tg, char *ubuf)
{
	struct repep *peer = peer_repep (ep);
	struct rrhdr *pg = get_rrhdr (ubuf);
	struct tgtd *back = route_backward (peer, ubuf);

	if (!back)
		return -1;
	pg->ttl--;
	skbuf_head_in (&get_rr_tgtd (back)->ls_head, ubuf);
	tgtd_try_enable_out (back);
	DEBUG_OFF ("ep %d resp %10.10s from socket %d", ep->eid, ubuf, tg->fd);
	return 0;
}

static int receiver_rm (struct epbase *ep, struct tgtd *tg, char **ubuf)
{
	if (skbuf_head_empty (&get_rr_tgtd (tg)->ls_head) ) {
		tgtd_try_disable_out (tg);
		return -1;
	}
	skbuf_head_out (&get_rr_tgtd (tg)->ls_head, *ubuf);
	DEBUG_OFF ("ep %d resp %10.10s to socket %d", ep->eid, *ubuf, tg->fd);
	return 0;
}

int epbase_proxyto (struct epbase *repep, struct epbase *reqep)
{
	struct repep *frontend = rep_ep (repep);
	struct reqep *backend = req_ep (reqep);

	dlock (repep, reqep);
	if (!list_empty (&repep->connectors) || !list_empty (&repep->bad_socks) ) {
		errno = EINVAL;
		dunlock (repep, reqep);
		return -1;
	}
	if (!list_empty (&reqep->connectors) || !list_empty (&reqep->bad_socks) ) {
		errno = EINVAL;
		dunlock (repep, reqep);
		return -1;
	}
	frontend->peer = backend;
	backend->peer = frontend;

	repep->vfptr.add = receiver_add;
	repep->vfptr.rm = receiver_rm;
	reqep->vfptr.add = dispatcher_add;
	reqep->vfptr.rm = dispatcher_rm;

	dunlock (repep, reqep);
	return 0;
}
