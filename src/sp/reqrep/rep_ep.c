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

static struct epbase *repep_alloc() {
	struct repep *repep = TNEW (struct repep);

	if (repep) {
		epbase_init (&repep->base);
		repep->peer = 0;
		return &repep->base;
	}
	return 0;
}

static void repep_destroy (struct epbase *ep)
{
	struct repep *repep = cont_of (ep, struct repep, base);
	BUG_ON (!repep);
	epbase_exit (ep);
	mem_free (repep, sizeof (*repep) );
}

static int repep_send (struct epbase *ep, char *ubuf)
{
	int rc = -1;
	struct rrhdr *pg = get_rrhdr (ubuf);
	struct rtentry *rt = rt_cur (ubuf);
	struct tgtd *tg = 0;

	mutex_lock (&ep->lock);
	get_tgtd_if (tg, &ep->connectors, !uuid_compare (get_rrtgtd (tg)->uuid, rt->uuid) );
	if (tg)
		list_move (&tg->item, &ep->connectors);
	mutex_unlock (&ep->lock);

	pg->go = 0;
	pg->end_ttl = pg->ttl;

	if (tg) {
		rc = xsend (tg->fd, ubuf);
		DEBUG_OFF ("ep %d send resp %10.10s to socket %d", ep->eid, ubuf, tg->fd);
	}
	return rc;
}

static int repep_add (struct epbase *ep, struct tgtd *tg, char *ubuf)
{
	struct rtentry *rt = rt_cur (ubuf);

	if (uuid_compare (rt->uuid, get_rrtgtd (tg)->uuid) )
		uuid_copy (get_rrtgtd (tg)->uuid, rt->uuid);
	DEBUG_OFF ("ep %d recv req %10.10s from socket %d", ep->eid, ubuf, tg->fd);
	mutex_lock (&ep->lock);
	skbuf_head_in (&ep->rcv, ubuf);
	BUG_ON (ep->rcv.waiters < 0);
	if (ep->rcv.waiters)
		condition_broadcast (&ep->cond);
	mutex_unlock (&ep->lock);
	return 0;
}

static int repep_rm (struct epbase *ep, struct tgtd *tg, char **ubuf)
{
	int rc = -1;
	if (tg->ent.events & XPOLLOUT)
		sg_update_tg (tg, tg->ent.events & ~XPOLLOUT);
	return rc;
}

static struct tgtd *repep_join (struct epbase *ep, int fd) {
	struct rrtgtd *rr_tg = TNEW (struct rrtgtd);

	if (!rr_tg)
		return 0;
	skbuf_head_init (&rr_tg->ls_head, SP_SNDWND);
	generic_tgtd_init (ep, &rr_tg->tg, fd);
	return &rr_tg->tg;
}

static void repep_term (struct epbase *ep, struct tgtd *tg)
{
	rrtgtd_free (get_rrtgtd (tg) );
}


static int set_proxyto (struct epbase *ep, void *optval, int optlen)
{
	int rc, backend_eid = * (int *) optval;
	struct epbase *peer = eid_get (backend_eid);

	if (!peer) {
		errno = EBADF;
		return -1;
	}
	rc = epbase_proxyto (ep, peer);
	eid_put (backend_eid);
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

static int repep_setopt (struct epbase *ep, int opt, void *optval, int optlen)
{
	int rc;
	if (opt < 0 || opt >= NELEM (setopt_vfptr, ep_setopt) || !setopt_vfptr[opt]) {
		errno = EINVAL;
		return -1;
	}
	rc = setopt_vfptr[opt] (ep, optval, optlen);
	return rc;
}

static int repep_getopt (struct epbase *ep, int opt, void *optval, int *optlen)
{
	int rc;
	if (opt < 0 || opt >= NELEM (getopt_vfptr, ep_getopt) || !getopt_vfptr[opt]) {
		errno = EINVAL;
		return -1;
	}
	rc = getopt_vfptr[opt] (ep, optval, optlen);
	return rc;
}

struct epbase_vfptr repep = {
	.sp_family = SP_REQREP,
	.sp_type = SP_REP,
	.alloc = repep_alloc,
	.destroy = repep_destroy,
	.send = repep_send,
	.add = repep_add,
	.rm = repep_rm,
	.join = repep_join,
	.term = repep_term,
	.setopt = repep_setopt,
	.getopt = repep_getopt,
};

struct epbase_vfptr *repep_vfptr = &repep;






