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

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "i64_rb.h"
#include "taskpool.h"
#include "timer.h"
#include "eventloop.h"

int eloop_init (eloop_t *el, int size, int max_io_events, int max_to)
{
	if ( (el->efd = epoll_create (size) ) < 0)
		return -1;
	if (! (el->ev_buf = NTNEW (struct epoll_event, max_io_events) ) ) {
		close (el->efd);
		return -1;
	}
	el->max_to = max_to;
	i64_rb_init (&el->ev_tree);
	mutex_init (&el->mutex);
	el->max_io_events = max_io_events;
	return 0;
}

int eloop_destroy (eloop_t *el)
{
	ev_t *ev = NULL;
	mutex_destroy (&el->mutex);
	while (!i64_rb_empty (&el->ev_tree) ) {
		ev = (ev_t *) (i64_rb_min (&el->ev_tree) )->data;
		i64_rb_delete (&el->ev_tree, &ev->rbe);
	}
	if (el->efd > 0)
		close (el->efd);
	if (el->ev_buf)
		mem_free (el->ev_buf, sizeof (*el->ev_buf) * el->max_io_events);
	return 0;
}


int eloop_add (eloop_t *el, ev_t *ev)
{
	int rc = 0;
	int64_t _cur_nsec = gettimeof (ns);
	struct epoll_event ee = {};

	if (ev->to_nsec > 0) {
		ev->rbe.key = _cur_nsec + ev->to_nsec;
		mutex_lock (&el->mutex);
		i64_rb_insert (&el->ev_tree, &ev->rbe);
		mutex_unlock (&el->mutex);
	}
	if (ev->fd >= 0) {
		ee.events = ev->events;
		ee.data.ptr = ev;
		if ( (rc = epoll_ctl (el->efd, EPOLL_CTL_ADD, ev->fd, &ee) ) == 0)
			el->event_size++;
	}
	return rc;
}


int eloop_mod (eloop_t *el, ev_t *ev)
{
	int rc = 0;
	int64_t _cur_nsec = gettimeof (ns);
	struct epoll_event ee = {};

	if (ev->to_nsec > 0) {
		mutex_lock (&el->mutex);
		if (ev->rbe.key)
			i64_rb_delete (&el->ev_tree, &ev->rbe);
		ev->rbe.key = _cur_nsec + ev->to_nsec;
		i64_rb_insert (&el->ev_tree, &ev->rbe);
		mutex_unlock (&el->mutex);
	}

	if (ev->fd >= 0) {
		ee.events = ev->events;
		ee.data.ptr = ev;
		rc = epoll_ctl (el->efd, EPOLL_CTL_MOD, ev->fd, &ee);
	}
	return rc;
}

int eloop_del (eloop_t *el, ev_t *ev)
{
	int rc = 0;
	struct epoll_event ee = {};

	if (ev->to_nsec > 0 && ev->rbe.key) {
		mutex_lock (&el->mutex);
		i64_rb_delete (&el->ev_tree, &ev->rbe);
		ev->rbe.key = 0;
		mutex_unlock (&el->mutex);
	}

	if (ev->fd >= 0) {
		ee.events = ev->events;
		ee.data.ptr = ev;
		if ( (rc = epoll_ctl (el->efd, EPOLL_CTL_DEL, ev->fd, &ee) ) == 0)
			el->event_size--;
	}
	return rc;
}

static void eloop_update_timer (eloop_t *el, ev_t *ev, int64_t cur)
{
	if (!ev->to_nsec || !ev->rbe.key)
		return;
	mutex_lock (&el->mutex);
	i64_rb_delete (&el->ev_tree, &ev->rbe);
	ev->rbe.key = cur + ev->to_nsec;
	i64_rb_insert (&el->ev_tree, &ev->rbe);
	mutex_unlock (&el->mutex);
}

static int64_t eloop_find_timer (eloop_t *el, int64_t to)
{
	int rc;
	struct i64_rbe *node = NULL;

	mutex_lock (&el->mutex);
	if (i64_rb_empty (&el->ev_tree)) {
		mutex_unlock (&el->mutex);
		return to;
	}
	node = i64_rb_min (&el->ev_tree);
	mutex_unlock (&el->mutex);

	if (node->key < to)
		to = node->key;
	return to;
}

static int __eloop_wait (eloop_t *el)
{
	int n = 0, size = el->max_io_events;
	int64_t max_to = el->max_to;
	ev_t *ev = NULL;
	struct epoll_event *ev_buf = el->ev_buf;
	struct i64_rbe *node = NULL;
	int64_t to = 0, _cur_nsec = gettimeof (ns);

	to = _cur_nsec + max_to * 1000000;
	max_to = eloop_find_timer (el, to);
	max_to = max_to > _cur_nsec ? max_to - _cur_nsec : 0;
	if (el->event_size <= 0)
		usleep (max_to/1000);
	else if ( (n = epoll_wait (el->efd, ev_buf, size, max_to/1000000) ) < 0)
		n = 0;
	while (ev_buf < el->ev_buf + n) {
		ev = (ev_t *) ev_buf->data.ptr;
		DEBUG_OFF ("fd %d happen with events %u", ev->fd, ev->events);
		eloop_update_timer (el, ev, _cur_nsec);
		ev_buf++;
	}

	mutex_lock (&el->mutex);
	size -= n;
	while (size && !i64_rb_empty (&el->ev_tree)) {
		node = i64_rb_min (&el->ev_tree);
		if (node->key > _cur_nsec)
			break;
		ev = (ev_t *) node->data;
		i64_rb_delete (&el->ev_tree, &ev->rbe);
		ev->rbe.key = 0;
		ev_buf->data.ptr = ev;
		ev_buf->events = EPOLLTIMEOUT;
		size--;
		ev_buf++;
	}
	mutex_unlock (&el->mutex);

	return ev_buf - el->ev_buf;
}


#define epoll_str_case(token)			\
    case token:					\
    str = #token;				\
    break					\
 
static char *epoll_str (u32 events)
{
	char *str = 0;

	switch (events) {
		epoll_str_case (EPOLLIN|EPOLLOUT|EPOLLERR|EPOLLRDHUP);
		epoll_str_case (EPOLLIN);
		epoll_str_case (EPOLLOUT);
		epoll_str_case (EPOLLERR);
		epoll_str_case (EPOLLRDHUP);
		epoll_str_case (EPOLLIN|EPOLLOUT);
		epoll_str_case (EPOLLIN|EPOLLERR);
		epoll_str_case (EPOLLOUT|EPOLLERR);
		epoll_str_case (EPOLLIN|EPOLLRDHUP);
		epoll_str_case (EPOLLOUT|EPOLLRDHUP);
		epoll_str_case (EPOLLERR|EPOLLRDHUP);
		epoll_str_case (EPOLLIN|EPOLLOUT|EPOLLERR);
		epoll_str_case (EPOLLIN|EPOLLOUT|EPOLLRDHUP);
		epoll_str_case (EPOLLIN|EPOLLERR|EPOLLRDHUP);
		epoll_str_case (EPOLLOUT|EPOLLERR|EPOLLRDHUP);
	}
	return str;
}

int eloop_once (eloop_t *el)
{
	int n = 0;
	ev_t *et;
	u32 happ;
	struct epoll_event *ev_buf = el->ev_buf;

	if ( (n = __eloop_wait (el) ) < 0)
		return -1;
	DEBUG_OFF ("%d fd has events", n);
	while (ev_buf < el->ev_buf + n) {
		et = (ev_t *) ev_buf->data.ptr;
		happ = et->happened = ev_buf->events;
		DEBUG_OFF ("fd %d waitup with events %s", et->fd, epoll_str (happ) );
		et->f (el, et);
		ev_buf++;
	}
	return 0;
}


int eloop_start (eloop_t *el)
{
	el->stopping = false;
	while (!el->stopping)
		eloop_once (el);
	return 0;
}

void eloop_stop (eloop_t *el)
{
	el->stopping = true;
}
