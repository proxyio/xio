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

#include "bus.h"

static struct epbase *bus_ep_alloc() {
	struct bus_ep *bus_ep = mem_zalloc (sizeof (struct bus_ep));

	if (bus_ep) {
		epbase_init (&bus_ep->base);
	}
	return &bus_ep->base;
}

static void bus_ep_destroy (struct epbase *ep)
{
	struct bus_ep *bus_ep = cont_of (ep, struct bus_ep, base);
	epbase_exit (ep);
	mem_free (bus_ep, sizeof (*bus_ep));
}

/* Send message to all targets in the connectors list_head. here just save
   the message into local storage head. */
static int bus_ep_send (struct epbase *ep, char *ubuf)
{
	int rc = 0;
	char *tmp;
	struct tgtd *dst;

	mutex_lock (&ep->lock);
	walk_tgtd (dst, &ep->connectors) {
		tmp = ubuf;
		/* for the last target. send the ubuf directly */
		if (list_next (&dst->item) != &ep->connectors)
			tmp = clone_ubuf (ubuf);
		msgbuf_head_in (&get_bus_tgtd (dst)->ls_head, tmp);
		tgtd_try_enable_out (dst);
	}
	mutex_unlock (&ep->lock);
	return rc;
}


/* Recv message from src target and dispatch its copy to other targets
   in the connectors list_head. */
static int bus_ep_add (struct epbase *ep, struct tgtd *src, char *ubuf)
{
	int rc;
	char *tmp;
	struct tgtd *tg;

	mutex_lock (&ep->lock);
	walk_tgtd (tg, &ep->connectors) {
		/* Don't backward */
		if (tg == src)
			continue;
		tmp = clone_ubuf (ubuf);
		msgbuf_head_in (&get_bus_tgtd (tg)->ls_head, tmp);
		tgtd_try_enable_out (tg);
	}
	msgbuf_head_in (&ep->rcv, ubuf);
	if (ep->rcv.waiters)
		condition_broadcast (&ep->cond);
	mutex_unlock (&ep->lock);
	return 0;
}

static int bus_ep_rm (struct epbase *ep, struct tgtd *tg, char **ubuf)
{
	mutex_lock (&ep->lock);
	if (msgbuf_head_empty (&get_bus_tgtd (tg)->ls_head)) {
		tgtd_try_disable_out (tg);
		mutex_unlock (&ep->lock);
		return -1;
	}
	msgbuf_head_out (&get_bus_tgtd (tg)->ls_head, ubuf);
	mutex_unlock (&ep->lock);
	return 0;
}

static struct tgtd *bus_ep_join (struct epbase *ep, int fd)
{
	struct bus_tgtd *ps_tg = mem_zalloc (sizeof (struct bus_tgtd));

	if (!ps_tg)
		return 0;
	msgbuf_head_init (&ps_tg->ls_head, SP_SNDWND);
	generic_tgtd_init (ep, &ps_tg->tg, fd);
	return &ps_tg->tg;
}

static void bus_ep_term (struct epbase *ep, struct tgtd *tg)
{
	bus_tgtd_free (get_bus_tgtd (tg));
}



static const ep_setopt setopt_vfptr[] = {
	0,
};

static const ep_getopt getopt_vfptr[] = {
	0,
};

static int bus_ep_setopt (struct epbase *ep, int opt, void *optval, int optlen)
{
	int rc;
	if (opt < 0 || opt >= NELEM (setopt_vfptr, ep_setopt) || !setopt_vfptr[opt]) {
		ERRNO_RETURN (EINVAL);
	}
	rc = setopt_vfptr[opt] (ep, optval, optlen);
	return rc;
}

static int bus_ep_getopt (struct epbase *ep, int opt, void *optval, int *optlen)
{
	int rc;
	if (opt < 0 || opt >= NELEM (getopt_vfptr, ep_getopt) || !getopt_vfptr[opt]) {
		ERRNO_RETURN (EINVAL);
	}
	rc = getopt_vfptr[opt] (ep, optval, optlen);
	return rc;
}

static struct epbase_vfptr bus_epbase = {
	.sp_family = SP_BUS,
	.sp_type = SP_BUS,
	.alloc = bus_ep_alloc,
	.destroy = bus_ep_destroy,
	.send = bus_ep_send,
	.add = bus_ep_add,
	.rm = bus_ep_rm,
	.join = bus_ep_join,
	.term = bus_ep_term,
	.setopt = bus_ep_setopt,
	.getopt = bus_ep_getopt,
};

struct epbase_vfptr *busep_vfptr = &bus_epbase;

