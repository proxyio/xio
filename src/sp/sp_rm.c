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

/* Term_by_tgtd() only called by backend xio worker when xsocket's POLLERR event
   happen. it used to be usual to destroy it. */
int sp_generic_term_by_tgtd(struct epbase *ep, struct tgtd *tg)
{
    int rc = 0;
    mutex_lock(&ep->lock);
    /* Move the bad tgtd into bad_socks head. if the tgtd is already in bad_socks
     * head, it seems we do the duplicate work here, that's fine, just delete and
     * insert one elem into the same list_head. if the tgtd is normal status. here
     * will remove it from the listeners head or connectors head and then insert
     * into bad_socks head.
     */
    list_move_tail(&tg->item, &ep->bad_socks);
    mutex_unlock(&ep->lock);
    return rc;
}


/* Term the target socket by fd. only called by sp_rm() API
   WARNING: this target socket maybe already on bad_status, see term_by_tgtd() */
int sp_generic_term_by_fd(struct epbase *ep, int fd)
{
    int rc = 0;
    struct tgtd *tg = 0;
    mutex_lock(&ep->lock);

    /* It seems unlikely that have two tgtd with the same fd in the list.
     * but, who know that!!
     */
    while ((tg = get_tgtd_if(tg, &ep->connectors, tg->fd == fd))) {
	tg->bad_status = true;
	list_move_tail(&tg->item, &ep->bad_socks);
    }
    while ((tg = get_tgtd_if(tg, &ep->listeners, tg->fd == fd))) {
	tg->bad_status = true;
	list_move_tail(&tg->item, &ep->bad_socks);
    }
    mutex_unlock(&ep->lock);
    return rc;
}

int sp_rm(int eid, int fd)
{
    struct epbase *ep = eid_get(eid);
    int rc;

    if (!ep) {
        errno = EBADF;
        return -1;
    }
    rc = ep->vfptr.term (ep, 0, fd);
    return rc;
}
