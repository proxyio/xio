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


int sp_send(int eid, char *xmsg) {
    struct epbase *ep = eid_get(eid);
    struct xmsg *out = cont_of(xmsg, struct xmsg, vec.xiov_base);
    
    if (!ep) {
	errno = EBADF;
	return -1;
    }
    mutex_lock(&ep->lock);
    while (!list_empty(&ep->snd.head) && ep->snd.size >= ep->snd.wnd
	   && ep->status != EP_SHUTDOWN) {
	ep->snd.waiters++;
	condition_wait(&ep->cond, &ep->lock);
	ep->snd.waiters--;
    }
    if (ep->status == EP_SHUTDOWN) {
	mutex_unlock(&ep->lock);
	errno = EBADF;
	return -1;
    }

    list_add_tail(&out->item, &ep->snd.head);
    ep->snd.size += xmsglen(xmsg);
    mutex_unlock(&ep->lock);
    eid_put(eid);
    return 0;
}
