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
#include "runner/taskpool.h"
#include "timesz.h"
#include "eventloop.h"
#include "ds/skrb_sync.h"

int eloop_add(eloop_t *el, ev_t *ev) {
    int rc = 0;
    int64_t _cur_nsec = rt_nstime();
    struct epoll_event ee = {};

    if (ev->to_nsec > 0) {
	ev->tr_node.key = _cur_nsec + ev->to_nsec;
	skrb_mutex_insert(&el->tr_tree, &ev->tr_node, &el->mutex);
    }
    if (ev->fd >= 0) {
	ee.events = ev->events;
	ee.data.ptr = ev;
	if ((rc = epoll_ctl(el->efd, EPOLL_CTL_ADD, ev->fd, &ee)) == 0)
	    el->event_size++;
    }
    return rc;
}


int eloop_mod(eloop_t *el, ev_t *ev) {
    int rc = 0;
    int64_t _cur_nsec = rt_nstime();
    struct epoll_event ee = {};

    if (ev->to_nsec > 0) {
	if (ev->tr_node.key)
	    skrb_mutex_delete(&el->tr_tree, &ev->tr_node, &el->mutex);
	ev->tr_node.key = _cur_nsec + ev->to_nsec;
	skrb_mutex_insert(&el->tr_tree, &ev->tr_node, &el->mutex);
    }

    if (ev->fd >= 0) {
	ee.events = ev->events;
	ee.data.ptr = ev;
	rc = epoll_ctl(el->efd, EPOLL_CTL_MOD, ev->fd, &ee);
    }
    return rc;
}

int eloop_del(eloop_t *el, ev_t *ev) {
    int rc = 0;
    struct epoll_event ee = {};

    if (ev->to_nsec > 0 && ev->tr_node.key) {
	skrb_mutex_delete(&el->tr_tree, &ev->tr_node, &el->mutex);
	ev->tr_node.key = 0;
    }

    if (ev->fd >= 0) {
	ee.events = ev->events;
	ee.data.ptr = ev;
	if ((rc = epoll_ctl(el->efd, EPOLL_CTL_DEL, ev->fd, &ee)) == 0)
	    el->event_size--;
    }
    return rc;
}

static void eloop_update_timer(eloop_t *el, ev_t *ev, int64_t cur) {
    if (!ev->to_nsec || !ev->tr_node.key)
	return;
    skrb_mutex_delete(&el->tr_tree, &ev->tr_node, &el->mutex);
    ev->tr_node.key = cur + ev->to_nsec;
    skrb_mutex_insert(&el->tr_tree, &ev->tr_node, &el->mutex);
}

static int64_t eloop_find_timer(eloop_t *el, int64_t to) {
    skrb_node_t *node = NULL;
    
    if (skrb_mutex_empty(&el->tr_tree, &el->mutex))
	return to;
    node = skrb_mutex_min(&el->tr_tree, &el->mutex);
    if (node->key < to)
	return node->key;
    return to;
}

static int __eloop_wait(eloop_t *el) {
    int n = 0, size = el->max_io_events;
    int64_t max_to = el->max_to;
    ev_t *ev = NULL;
    struct epoll_event *ev_buf = el->ev_buf;
    skrb_node_t *node = NULL;
    int64_t to = 0, _cur_nsec = rt_nstime();
    
    to = _cur_nsec + max_to * 1000000;
    max_to = eloop_find_timer(el, to);
    max_to = max_to > _cur_nsec ? max_to - _cur_nsec : 0;
    if (el->event_size <= 0)
	rt_usleep(max_to/1000);
    else if ((n = epoll_wait(el->efd, ev_buf, size, max_to/1000000)) < 0)
	n = 0;
    while (ev_buf < el->ev_buf + n) {
	DEBUG_OFF("fd %d happen with events", ev_buf->events);
	eloop_update_timer(el, (ev_t *)ev_buf->data.ptr, _cur_nsec);
	ev_buf++;
    }
    
    size -= n;
    while (size && !skrb_mutex_empty(&el->tr_tree, &el->mutex)) {
	node = skrb_mutex_min(&el->tr_tree, &el->mutex);
	if (node->key > _cur_nsec)
	    break;
	ev = (ev_t *)node->data;
	skrb_mutex_delete(&el->tr_tree, &ev->tr_node, &el->mutex);
	ev->tr_node.key = 0;
	ev_buf->data.ptr = ev;
	ev_buf->events = EPOLLTIMEOUT;
	size--;
	ev_buf++;
    }
    return ev_buf - el->ev_buf;
}


int eloop_once(eloop_t *el) {
    int n = 0;
    ev_t *et;
    struct epoll_event *ev_buf = el->ev_buf;
    
    if ((n = __eloop_wait(el)) < 0)
	return -1;
    DEBUG_OFF("%d fd has events", n);
    while (ev_buf < el->ev_buf + n) {
	et = (ev_t *)ev_buf->data.ptr;
	et->happened = ev_buf->events;
	DEBUG_OFF("fd %d eventloop with events %d", et->fd, et->happened);
	et->f(el, et);
	ev_buf++;
    }
    return 0;
}


int eloop_start(eloop_t *el) {
    el->stopping = false;
    while (!el->stopping)
	eloop_once(el);
    return 0;
}

void eloop_stop(eloop_t *el) {
    el->stopping = true;
}
