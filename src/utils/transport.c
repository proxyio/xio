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


static struct list_head transport_vfs = {};
struct list_head *transport_vfptr_head = &transport_vfs;

/* Following transpotr vfptr are provided */
extern struct transport_vf *tcp_vfptr;
extern struct transport_vf *ipc_vfptr;

#define walk_transpotr_vfptr_s(tp_vfptr)				\
    walk_each_entry(tp_vfptr, &transport_vfs, struct transport_vf, item)

void transport_module_init() {
    INIT_LIST_HEAD(&transport_vfs);
    list_add(&tcp_vfptr->item, transport_vfptr_head);
    if (tcp_vfptr->init)
	tcp_vfptr->init();
    list_add(&ipc_vfptr->item, transport_vfptr_head);
    if (ipc_vfptr->init)
	ipc_vfptr->init();
}

void transport_module_exit() {
    struct transport_vf *tp_vfptr;

    walk_transpotr_vfptr_s(tp_vfptr) {
	if (tp_vfptr->exit)
	    tp_vfptr->exit();
    }
}

struct transport_vf *transport_lookup(int pf) {
    struct transport_vf *tp;

    walk_transpotr_vfptr_s(tp) {
	if (tp->proto == pf)
	    return tp;
    }
    return 0;
}
