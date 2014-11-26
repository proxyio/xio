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

int sp_recv(int eid, char** ubuf) {
    struct epbase* ep = eid_get(eid);

    if (!ep) {
        ERRNO_RETURN(EBADF);
    }

    mutex_lock(&ep->lock);

    /* All the received message would saved in the rcv.head. if the endpoint
     * status ok and the rcv.head is empty, we wait here. when the endpoint
     * status is bad or has messages come, the wait return.
     * TODO: can condition_wait support timeout.
     */
    while (!ep->status.shutdown && list_empty(&ep->rcv.head)) {
        ep->rcv.waiters++;
        condition_wait(&ep->cond, &ep->lock);
        ep->rcv.waiters--;
    }

    /* Check the endpoint status, maybe it's bad */
    if (ep->status.shutdown) {
        mutex_unlock(&ep->lock);
        eid_put(eid);
        ERRNO_RETURN(EBADF);
    }

    msgbuf_head_out(&ep->rcv, ubuf);

    mutex_unlock(&ep->lock);
    eid_put(eid);
    return 0;
}
