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

#include "pub.h"

static struct epbase *pub_ep_alloc() {
	struct pub_ep *pub_ep = TNEW (struct pub_ep);

	if (pub_ep) {
		epbase_init (&pub_ep->base);
	}
	return &pub_ep->base;
}

static void pub_ep_destroy (struct epbase *ep)
{
	struct pub_ep *pub_ep = cont_of (ep, struct pub_ep, base);
	epbase_exit (ep);
	mem_free (pub_ep, sizeof (*pub_ep) );
}

static int pub_ep_send (struct epbase *ep, char *ubuf)
{
	int rc = 0;
	char *dst;
	struct tgtd *tg;

	mutex_lock (&ep->lock);
	walk_tgtd (tg, &ep->connectors) {
		dst = ubuf;
		if (list_next (&tg->item) != &ep->connectors)
			BUG_ON ((rc = ubufctl (ubuf, SCLONE, &dst)));
		skbuf_head_in (&get_pubsub_tgtd (tg)->ls_head, dst);
		tgtd_try_enable_out (tg);
	}
	mutex_unlock (&ep->lock);
	return rc;
}

/* limited by the pubsub protocol, the sub endpoint can't send any
   message to pub endpoint, if it do it, we can mark this socket on
   bad status, or drop the message simply. */
static int pub_ep_add (struct epbase *ep, struct tgtd *tg, char *ubuf)
{
	int rc = -1;

	if (rc)
		errno = EPERM;
	return rc;
}

static int pub_ep_rm (struct epbase *ep, struct tgtd *tg, char **ubuf)
{
	mutex_lock (&ep->lock);
	if (skbuf_head_empty (&get_pubsub_tgtd (tg)->ls_head)) {
		tgtd_try_disable_out (tg);
		mutex_unlock (&ep->lock);
		return -1;
	}
	skbuf_head_out (&get_pubsub_tgtd (tg)->ls_head, *ubuf);
	mutex_unlock (&ep->lock);
	return 0;
}

static struct tgtd *pub_ep_join (struct epbase *ep, int fd)
{
	struct pubsub_tgtd *ps_tg = TNEW(struct pubsub_tgtd);

	if (!ps_tg)
		return 0;
	skbuf_head_init (&ps_tg->ls_head, SP_SNDWND);
	generic_tgtd_init (ep, &ps_tg->tg, fd);
	return &ps_tg->tg;
}

static void pub_ep_term (struct epbase *ep, struct tgtd *tg)
{
	pubsub_tgtd_free (get_pubsub_tgtd (tg) );
}



static const ep_setopt setopt_vfptr[] = {
	0,
};

static const ep_getopt getopt_vfptr[] = {
	0,
};

static int pub_ep_setopt (struct epbase *ep, int opt, void *optval, int optlen)
{
	int rc;
	if (opt < 0 || opt >= NELEM (setopt_vfptr, ep_setopt) || !setopt_vfptr[opt]) {
		errno = EINVAL;
		return -1;
	}
	rc = setopt_vfptr[opt] (ep, optval, optlen);
	return rc;
}

static int pub_ep_getopt (struct epbase *ep, int opt, void *optval, int *optlen)
{
	int rc;
	if (opt < 0 || opt >= NELEM (getopt_vfptr, ep_getopt) || !getopt_vfptr[opt]) {
		errno = EINVAL;
		return -1;
	}
	rc = getopt_vfptr[opt] (ep, optval, optlen);
	return rc;
}

static struct epbase_vfptr pub_epbase = {
	.sp_family = SP_PUBSUB,
	.sp_type = SP_PUB,
	.alloc = pub_ep_alloc,
	.destroy = pub_ep_destroy,
	.send = pub_ep_send,
	.add = pub_ep_add,
	.rm = pub_ep_rm,
	.join = pub_ep_join,
	.term = pub_ep_term,
	.setopt = pub_ep_setopt,
	.getopt = pub_ep_getopt,
};

struct epbase_vfptr *pubep_vfptr = &pub_epbase;

