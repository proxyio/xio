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
	struct i64_rbe tr_node;
	struct list_head el_link;
} ev_t;

#define list_for_each_et_safe(pos, tmp, head)				\
    list_for_each_entry_safe(pos, tmp, head, ev_t, el_link)

typedef struct eloop {
	int stopping;
	int efd, max_io_events, event_size;
	int64_t max_to;
	struct i64_rb tr_tree;
	mutex_t mutex;
	struct list_head link;
	struct epoll_event *ev_buf;
} eloop_t;

#define list_for_each_el_safe(pos, tmp, head)			\
    list_for_each_entry_safe(pos, tmp, head, eloop_t, link)


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

static inline int eloop_init (eloop_t *el, int size, int max_io_events, int max_to)
{
	if ( (el->efd = epoll_create (size) ) < 0)
		return -1;
	if (! (el->ev_buf = NTNEW (struct epoll_event, max_io_events) ) ) {
		close (el->efd);
		return -1;
	}
	el->max_to = max_to;
	i64_rb_init (&el->tr_tree);
	mutex_init (&el->mutex);
	el->max_io_events = max_io_events;
	return 0;
}

static inline int eloop_destroy (eloop_t *el)
{
	ev_t *ev = NULL;
	mutex_destroy (&el->mutex);
	while (!i64_rb_empty (&el->tr_tree) ) {
		ev = (ev_t *) (i64_rb_min (&el->tr_tree) )->data;
		i64_rb_delete (&el->tr_tree, &ev->tr_node);
	}
	if (el->efd > 0)
		close (el->efd);
	if (el->ev_buf)
		mem_free (el->ev_buf, sizeof (*el->ev_buf) * el->max_io_events);
	return 0;
}

int eloop_add (eloop_t *el, ev_t *ev);
int eloop_del (eloop_t *el, ev_t *ev);
int eloop_mod (eloop_t *el, ev_t *ev);
int eloop_once (eloop_t *el);
int eloop_start (eloop_t *el);
void eloop_stop (eloop_t *el);


#endif
