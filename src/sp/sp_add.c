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

#include <xio/sp.h>
#include "sp_module.h"

int epbase_add_tgtd (struct epbase *ep, struct tgtd *tg)
{
	mutex_lock (&ep->lock);

	/* Don't attach any target socket into bad status endpoint.
	 * we check the endpoint status here, for example, the endpoint
	 * was closed */
	if (ep->status.bad) {
		mutex_unlock(&ep->lock);
		return -1;
	}
	switch (get_socktype (tg->fd) ) {
	case XLISTENER:
		list_add_tail (&tg->item, &ep->listeners);
		ep->nlisteners++;
		break;
	case XCONNECTOR:
		list_add_tail (&tg->item, &ep->connectors);
		ep->nconnectors++;
		break;
	default:
		BUG_ON (1);
	}
	sg_add_tg (tg);
	mutex_unlock (&ep->lock);
}

void generic_tgtd_init (struct epbase *ep, struct tgtd *tg, int fd)
{
	int socktype = get_socktype (fd);

	tg->fd = fd;
	tg->owner = ep;
	tg->pollfd.fd = fd;
	tg->pollfd.self = tg;
	tg->pollfd.events = XPOLLIN|XPOLLERR;
	tg->pollfd.events |= socktype == XCONNECTOR ? XPOLLOUT : 0;
}

int sp_add (int eid, int fd)
{
	struct epbase *ep = eid_get (eid);
	int rc;
	int on = 1;
	struct tgtd *tg;

	if (!ep) {
		errno = EBADF;
		return -1;
	}
	if ( (tg = ep->vfptr.join (ep, fd) ) ) {
		if ((rc = epbase_add_tgtd (ep, tg))) {
			ep->vfptr.term (ep, tg);
			eid_put (eid);
			return -1;
		}
	}
	xsetopt (fd, XL_SOCKET, XNOBLOCK, &on, sizeof (on) );
	eid_put (eid);
	return 0;
}
