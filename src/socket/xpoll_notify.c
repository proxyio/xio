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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sync/waitgroup.h>
#include <runner/taskpool.h>
#include "xgb.h"

int xsock_check_events(struct xsock *xsk, int events) {
    int happened = 0;

    if (events & XPOLLIN) {
	if (xsk->type == XCONNECTOR)
	    happened |= !list_empty(&xsk->rcv_head) ? XPOLLIN : 0;
	else if (xsk->type == XLISTENER)
	    happened |= !list_empty(&xsk->acceptq.head) ? XPOLLIN : 0;
    }
    if (events & XPOLLOUT)
	happened |= can_send(xsk) ? XPOLLOUT : 0;
    if (events & XPOLLERR)
	happened |= !xsk->fok ? XPOLLERR : 0;
    DEBUG_OFF("%d happen %s", xsk->fd, xpoll_str[happened]);
    return happened;
}

/* Generic xpoll_t notify function. always called by xsock_protocol
 * when has any message come or can send any massage into network
 * or has a new connection wait for established.
 * here we only check the mq events and l4proto_spec saved the other
 * events gived by xsock_protocol
 */
void __xpoll_notify(struct xsock *xsk) {
    int happened = 0;
    struct xpoll_entry *ent, *nx;

    happened |= xsock_check_events(xsk, XPOLLIN|XPOLLOUT|XPOLLERR);
    xsock_walk_ent(ent, nx, &xsk->xpoll_head) {
	ent->notify->event(ent->notify, ent, ent->event.care & happened);
    }
}

void xpoll_notify(struct xsock *xsk) {
    mutex_lock(&xsk->lock);
    __xpoll_notify(xsk);
    mutex_unlock(&xsk->lock);
}
