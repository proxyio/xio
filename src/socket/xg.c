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
#include "xg.h"

/* Backend poller wait kernel timeout msec */
#define DEF_ELOOPTIMEOUT 1
#define DEF_ELOOPIOMAX 100

struct xglobal xgb = {};

struct sockbase_vfptr *sockbase_vfptr_lookup (int pf, int type) {
	struct sockbase_vfptr *vfptr, *ss;

	walk_sockbase_vfptr_s (vfptr, ss, &xgb.sockbase_vfptr_head) {
		if (pf == vfptr->pf && vfptr->type == type)
			return vfptr;
	}
	return 0;
}

extern struct sockbase_vfptr mix_listener_vfptr;

void socket_module_init()
{
	waitgroup_t wg;
	int fd;
	int i;
	struct list_head *protocol_head = &xgb.sockbase_vfptr_head;

	mutex_init (&xgb.lock);
	for (fd = 0; fd < PROXYIO_MAX_SOCKS; fd++)
		xgb.unused[fd] = fd;

	/* The priority of sockbase_vfptr: inproc > ipc > tcp */
	INIT_LIST_HEAD (protocol_head);
	list_add_tail (&inproc_listener_vfptr.item, protocol_head);
	list_add_tail (&inproc_connector_vfptr.item, protocol_head);
	list_add_tail (&ipc_listener_vfptr.item, protocol_head);
	list_add_tail (&ipc_connector_vfptr.item, protocol_head);
	list_add_tail (&tcp_listener_vfptr.item, protocol_head);
	list_add_tail (&tcp_connector_vfptr.item, protocol_head);
	list_add_tail (&mix_listener_vfptr.item, protocol_head);
}

void socket_module_exit()
{
	BUG_ON (xgb.nsockbases);
}
