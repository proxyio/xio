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
#include <utils/waitgroup.h>
#include <utils/taskpool.h>
#include "xgb.h"

int check_pollevents(struct sockbase *self, int events) {
    int happened = 0;

    if (events & XPOLLIN) {
	if (self->vfptr->type == XCONNECTOR)
	    happened |= !list_empty(&self->rcv.head) ? XPOLLIN : 0;
	else if (self->vfptr->type == XLISTENER)
	    happened |= !list_empty(&self->acceptq.head) ? XPOLLIN : 0;
    }
    if (events & XPOLLOUT)
	happened |= can_send(self) ? XPOLLOUT : 0;
    if (events & XPOLLERR)
	happened |= self->fepipe ? XPOLLERR : 0;
    DEBUG_OFF("%d happen %s", self->fd, xpoll_str[happened]);
    return happened;
}

/* Generic xpoll_t notify function. always called by sockbase_vfptr
 * when has any message come or can send any massage into network
 * or has a new connection wait for established.
 * here we only check the mq events and proto_spec saved the other
 * events gived by sockbase_vfptr
 */
void __emit_pollevents(struct sockbase *self) {
    int happened = 0;
    struct pollbase *pb, *npb;

    happened |= check_pollevents(self, XPOLLIN|XPOLLOUT|XPOLLERR);
    walk_pollbase_s(pb, npb, &self->poll_entries) {
	BUG_ON(!pb->vfptr);
	pb->vfptr->emit(pb, happened & pb->ent.events);
    }
}

void emit_pollevents(struct sockbase *self) {
    mutex_lock(&self->lock);
    __emit_pollevents(self);
    mutex_unlock(&self->lock);
}
