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
#include <utils/str_array.h>
#include <xsocket/xg.h>

static int mix_listener_bind (struct sockbase *sb, const char *sock)
{
	int i;
	int child_fd;
	struct sockbase *child_sb = 0;
	struct sockbase *tmp = 0;
	struct str_array sock_arr = {};

	str_array_init (&sock_arr);
	str_split (sock, &sock_arr, "+");
	for (i = 0; i < sock_arr.size; i++) {
		if ((child_fd = xlisten (sock_arr.at[i])) < 0)
			goto BAD;
		if (!(child_sb = xget (child_fd)))
			goto BAD;
		list_add_tail (&child_sb->sib_link, &sb->sub_socks);
		child_sb->owner = sb;
		while (acceptq_rm_nohup (child_sb, &tmp) == 0)
			acceptq_add (sb, tmp);
		xput (child_fd);
	}
	return 0;
 BAD:
	walk_sub_sock (child_sb, tmp, &sb->sub_socks) {
		list_del_init (&child_sb->sib_link);
		xclose (child_sb->fd);
	}
	return -1;
}

static void mix_listener_close (struct sockbase *sb)
{
	struct sockbase *child_sb, *tmp;

	walk_sub_sock (child_sb, tmp, &sb->sub_socks) {
		child_sb->owner = 0;
		list_del_init (&child_sb->sib_link);
		xclose (child_sb->fd);
	}

	/* Destroy acceptq's connection */
	while (acceptq_rm_nohup (sb, &tmp) == 0) {
		xclose (tmp->fd);
	}

	sockbase_exit (sb);
	mem_free (sb, sizeof (*sb) );
}

static struct sockbase *mix_alloc() {
	struct mix_sock *self = TNEW (struct mix_sock);

	BUG_ON (!self);
	sockbase_init (&self->base);
	return &self->base;
}

struct sockbase_vfptr mix_listener_vfptr = {
	.type = XLISTENER,
	.pf = TP_MIX,
	.alloc = mix_alloc,
	.send = 0,
	.bind = mix_listener_bind,
	.close = mix_listener_close,
	.setopt = 0,
	.getopt = 0,
};
