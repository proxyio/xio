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

#include "base.h"
#include "tcp/tcp.h"
#include "ipc/ipc.h"
#include "transport.h"


static struct list_head transport_head = {};

/* Following transpotr vfptr are provided */
extern struct transport *tcp_vfptr;
extern struct transport *ipc_vfptr;

#define walk_transpotr_vfptr_s(tp_vfptr)				\
    walk_each_entry(tp_vfptr, &transport_head, struct transport, item)

void transport_module_init()
{
	INIT_LIST_HEAD (&transport_head);
	list_add (&tcp_vfptr->item, &transport_head);
	if (tcp_vfptr->init)
		tcp_vfptr->init();
	list_add (&ipc_vfptr->item, &transport_head);
	if (ipc_vfptr->init)
		ipc_vfptr->init();
}

void transport_module_exit()
{
	struct transport *tp_vfptr;

	walk_transpotr_vfptr_s (tp_vfptr) {
		if (tp_vfptr->exit)
			tp_vfptr->exit();
	}
}

struct transport *transport_lookup (int pf) {
	struct transport *tp = 0;

	walk_transpotr_vfptr_s (tp) {
		if (tp->proto == pf)
			return tp;
	}
	return 0;
}
