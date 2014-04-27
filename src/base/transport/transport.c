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
#include "transport/tcp/tcp.h"
#include "transport/ipc/ipc.h"
#include "transport/transport.h"


static struct list_head _transport_head = {};
struct list_head *transport_head = &_transport_head;

/* List all available transport here */
extern struct transport *tcp_transport;
extern struct transport *ipc_transport;

#define list_for_each_transport(tp)					\
    list_for_each_entry(tp, transport_head, struct transport, item)

static inline void add_transport(struct transport *tp) {
    tp->global_init();
    list_add(&tp->item, transport_head);
}

void global_transport_init() {
    INIT_LIST_HEAD(transport_head);
    add_transport(tcp_transport);
    add_transport(ipc_transport);
}

void global_transport_exit() {
}


struct transport *transport_lookup(int pf) {
    struct transport *tp = NULL;

    list_for_each_transport(tp)
	if (tp->proto == pf)
	    return tp;
    return NULL;
}
