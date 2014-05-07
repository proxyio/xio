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


int sp_add(int eid, int fd) {
    struct epbase *ep = eid_get(eid);
    struct epsk *nsk;
    int rc;
    int socktype = 0;
    int optlen = sizeof(socktype);
    
    rc = xgetopt(fd, XL_SOCKET, XSOCKTYPE, &socktype, &optlen);
    if (!ep || rc < 0) {
	errno = EBADF;
	eid_put(eid);
	return -1;
    }
    switch (socktype) {
    case XCONNECTOR:
	ep->vfptr->join(ep, 0, fd);
	break;
    case XLISTENER:
	if (!(nsk = epsk_new())) {
	    eid_put(eid);
	    return -1;
	}
	nsk->owner = ep;
	nsk->ent.xd = fd;
	nsk->ent.self = nsk;
	nsk->ent.care = XPOLLIN|XPOLLERR;
	mutex_lock(&ep->lock);
	list_add_tail(&nsk->item, &ep->listeners);
	mutex_unlock(&ep->lock);
	sg_add_sk(nsk);
	break;
    default:
	BUG_ON(1);
    }
    eid_put(eid);
    return 0;
}
