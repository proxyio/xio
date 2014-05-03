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
#include <runner/taskpool.h>
#include "xgb.h"

extern int _xlisten(int pf, const char *addr);


static void xmultiple_close(int fd) {
    struct xsock *sub, *nx;
    struct xsock *self = xget(fd);

    xsock_walk_sub_socks(sub, nx, &self->sub_socks) {
	sub->owner = -1;
	list_del_init(&sub->sib_link);
	xclose(sub->fd);
    }
}

static int xmul_listener_bind(int fd, const char *sock) {
    struct xsock_protocol *l4proto, *nx;
    struct xsock *self = xget(fd), *sub;
    int sub_fd;
    int pf = self->pf;

    xsock_protocol_walk_safe(l4proto, nx, &xgb.xsock_protocol_head) {
	if (!(pf & l4proto->pf) || l4proto->type != XLISTENER)
	    continue;
	pf &= ~l4proto->pf;
	if ((sub_fd = _xlisten(l4proto->pf, sock)) < 0)
	    goto BAD;
	sub = xget(sub_fd);
	sub->owner = fd;
	list_add_tail(&sub->sib_link, &self->sub_socks);
    }
    if (!list_empty(&self->sub_socks))
	return 0;
 BAD:
    xmultiple_close(fd);
    return -1;
}

static void xmul_listener_close(int fd) {
    struct xsock *self = xget(fd);
    xmultiple_close(fd);
    xsock_free(self);
    DEBUG_OFF("xsock %d multiple_close", fd);
}

struct xsock_protocol xmul_listener_protocol[4] = {
    {
	.type = XLISTENER,
	.pf = XPF_TCP|XPF_IPC,
	.bind = xmul_listener_bind,
	.close = xmul_listener_close,
	.setsockopt = 0,
	.getsockopt = 0,
	.notify = 0,
    },
    {
	.type = XLISTENER,
	.pf = XPF_IPC|XPF_INPROC,
	.bind = xmul_listener_bind,
	.close = xmul_listener_close,
	.setsockopt = 0,
	.getsockopt = 0,
	.notify = 0,
    },
    {
	.type = XLISTENER,
	.pf = XPF_TCP|XPF_INPROC,
	.bind = xmul_listener_bind,
	.close = xmul_listener_close,
	.setsockopt = 0,
	.getsockopt = 0,
	.notify = 0,
    },
    {
	.type = XLISTENER,
	.pf = XPF_TCP|XPF_IPC|XPF_INPROC,
	.bind = xmul_listener_bind,
	.close = xmul_listener_close,
	.setsockopt = 0,
	.getsockopt = 0,
	.notify = 0,
    }
};
