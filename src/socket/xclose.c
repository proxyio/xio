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

static void xshutdown(struct xsock *self) {
    struct xcpu *cpu = xcpuget(self->cpu_no);
    struct xtask *ts = &self->shutdown;    

    mutex_lock(&cpu->lock);
    while (efd_signal(&cpu->efd) < 0) {
	/* Pipe is full and another thread is unsignaling. */
	mutex_unlock(&cpu->lock);
	mutex_lock(&cpu->lock);
    }
    if (!self->fclosed && !attached(&ts->link)) {
	self->fclosed = true;
	list_add_tail(&ts->link, &cpu->shutdown_socks);
    }
    mutex_unlock(&cpu->lock);
}

void xclose(int fd) {
    struct xsock *self = xget(fd);
    struct xpoll_t *po;
    struct xpoll_entry *ent, *nx;
    struct list_head poll_entries = {};

    INIT_LIST_HEAD(&poll_entries);
    mutex_lock(&self->lock);
    list_splice(&self->poll_entries, &poll_entries);
    mutex_unlock(&self->lock);

    xsock_walk_ent(ent, nx, &poll_entries) {
	po = cont_of(ent->notify, struct xpoll_t, notify);
	xpoll_ctl(po, XPOLL_DEL, &ent->event);
	__detach_from_xsock(ent);
	xent_put(ent);
    }

    xshutdown(self);
}
