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
#include <utils/taskpool.h>
#include "../xgb.h"

extern int _xlisten(int pf, const char *addr);

static int xmul_listener_bind(struct sockbase *sb, const char *sock)
{
	struct sockbase_vfptr *vfptr, *ss;
	struct sockbase *sub, *nsub, *new;
	int sub_fd;
	int pf = sb->vfptr->pf;
	struct list_head sub_socks;
	struct list_head new_socks;

	INIT_LIST_HEAD(&sub_socks);
	INIT_LIST_HEAD(&new_socks);
	walk_sockbase_vfptr_s(vfptr, ss, &xgb.sockbase_vfptr_head) {
		if (!(pf & vfptr->pf) || vfptr->type != XLISTENER)
			continue;
		pf &= ~vfptr->pf;
		if ((sub_fd = _xlisten(vfptr->pf, sock)) < 0)
			goto BAD;
		sub = xgb.sockbases[sub_fd];
		list_add_tail(&sub->sib_link, &sub_socks);
	}
	if (list_empty(&sub_socks))
		return -1;
	walk_sub_sock(sub, nsub, &sub_socks) {
		sub->owner = sb;
		while (acceptq_rm_nohup(sub, &new) == 0) {
			list_add_tail(&new->acceptq.link, &new_socks);
		}
	}
	mutex_lock(&sb->lock);
	list_splice(&new_socks, &sb->acceptq.head);
	list_splice(&sub_socks, &sb->sub_socks);
	mutex_unlock(&sb->lock);
	return 0;
BAD:
	walk_sub_sock(sub, nsub, &sub_socks) {
		list_del_init(&sub->sib_link);
		xclose(sub->fd);
	}
	return -1;
}

static void xmul_listener_close(struct sockbase *sb)
{
	struct sockbase *nsb;
	struct sockbase *sub, *nx;

	walk_sub_sock(sub, nx, &sb->sub_socks) {
		sub->owner = 0;
		list_del_init(&sub->sib_link);
		xclose(sub->fd);
	}

	/* Destroy acceptq's connection */
	while (acceptq_rm_nohup(sb, &nsb) == 0) {
		xclose(nsb->fd);
	}

	xsock_exit(sb);
	mem_free(sb, sizeof(*sb));
}

static struct sockbase *xmul_alloc() {
	struct sockbase *sb = TNEW(struct sockbase);

	if (sb)
		xsock_init(sb);
	return sb;
}

struct sockbase_vfptr xmul_listener_spec[4] = {
	{
		.type = XLISTENER,
		.pf = XPF_TCP|XPF_IPC,
		.alloc = xmul_alloc,
		.bind = xmul_listener_bind,
		.close = xmul_listener_close,
		.setopt = 0,
		.getopt = 0,
		.notify = 0,
	},
	{
		.type = XLISTENER,
		.pf = XPF_IPC|XPF_INPROC,
		.alloc = xmul_alloc,
		.bind = xmul_listener_bind,
		.close = xmul_listener_close,
		.setopt = 0,
		.getopt = 0,
		.notify = 0,
	},
	{
		.type = XLISTENER,
		.pf = XPF_TCP|XPF_INPROC,
		.alloc = xmul_alloc,
		.bind = xmul_listener_bind,
		.close = xmul_listener_close,
		.setopt = 0,
		.getopt = 0,
		.notify = 0,
	},
	{
		.type = XLISTENER,
		.pf = XPF_TCP|XPF_IPC|XPF_INPROC,
		.alloc = xmul_alloc,
		.bind = xmul_listener_bind,
		.close = xmul_listener_close,
		.setopt = 0,
		.getopt = 0,
		.notify = 0,
	}
};
