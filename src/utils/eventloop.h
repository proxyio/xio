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

#ifndef _H_PROXYIO_EVENTLOOP_
#define _H_PROXYIO_EVENTLOOP_

#include <unistd.h>
#include <sys/epoll.h>
#include "alloc.h"
#include "list.h"
#include "i64_rb.h"
#include "mutex.h"


#define EPOLLTIMEOUT (1 << 28)
#ifndef EPOLLRDHUP
#define EPOLLRDHUP 0x2000
#endif

struct ev;
struct eloop;
typedef int (*event_handler) (struct eloop *el, struct ev *et);

typedef struct ev {
	event_handler f;
	void *data;
	int fd;
	int64_t to_nsec;
	uint32_t events;
	uint32_t happened;
	struct i64_rbe rbe;
	struct list_head item;
} ev_t;

#define list_for_each_et_safe(pos, tmp, head)			\
    list_for_each_entry_safe(pos, tmp, head, ev_t, item)

typedef struct eloop {
	int stopping;
	int efd, max_io_events, event_size;
	int64_t max_to;
	struct i64_rb ev_tree;
	mutex_t mutex;
	struct epoll_event *ev_buf;
} eloop_t;

static inline ev_t *ev_new()
{
	ev_t *ev = TNEW (ev_t);
	return ev;
}

static inline eloop_t *eloop_new()
{
	eloop_t *el = TNEW (eloop_t);
	return el;
}

int eloop_init (eloop_t *el, int size, int max_io_events, int max_to);

int eloop_destroy (eloop_t *el);

int eloop_add (eloop_t *el, ev_t *ev);

int eloop_del (eloop_t *el, ev_t *ev);

int eloop_mod (eloop_t *el, ev_t *ev);

int eloop_once (eloop_t *el);

int eloop_start (eloop_t *el);

void eloop_stop (eloop_t *el);


#endif
