#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include "timesz.h"
#include "epoll.h"
#include "ds/skrb_sync.h"

int epoll_add(epoll_t *el, epollevent_t *ev) {
    int rc = 0;
    int64_t _cur_nsec = rt_nstime();
    struct epoll_event ee;

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


int epoll_mod(epoll_t *el, epollevent_t *ev) {
    int rc = 0;
    int64_t _cur_nsec = rt_nstime();
    struct epoll_event ee;

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

int epoll_del(epoll_t *el, epollevent_t *ev) {
    int rc = 0;
    struct epoll_event ee;

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

static void epoll_update_timer(epoll_t *el, epollevent_t *ev, int64_t cur) {
    if (!ev->to_nsec || !ev->tr_node.key)
	return;
    skrb_mutex_delete(&el->tr_tree, &ev->tr_node, &el->mutex);
    ev->tr_node.key = cur + ev->to_nsec;
    skrb_mutex_insert(&el->tr_tree, &ev->tr_node, &el->mutex);
}

static int64_t epoll_find_timer(epoll_t *el, int64_t to) {
    skrb_node_t *node = NULL;
    
    if (skrb_mutex_empty(&el->tr_tree, &el->mutex))
	return to;
    node = skrb_mutex_min(&el->tr_tree, &el->mutex);
    if (node->key < to)
	return node->key;
    return to;
}

static int __epoll_wait(epoll_t *el) {
    int n = 0, size = el->max_io_events;
    int64_t max_to = el->max_to;
    epollevent_t *ev = NULL;
    struct epoll_event *ev_buf = el->ev_buf;
    skrb_node_t *node = NULL;
    int64_t to = 0, _cur_nsec = rt_nstime();
    
    to = _cur_nsec + max_to * 1000000;
    max_to = epoll_find_timer(el, to);
    max_to = max_to > _cur_nsec ? max_to - _cur_nsec : 0;
    if (el->event_size <= 0)
	rt_usleep(max_to/1000);
    else if ((n = epoll_wait(el->efd, ev_buf, size, max_to/1000000)) < 0)
	n = 0;

    while (ev_buf < el->ev_buf + n) {
	epoll_update_timer(el, (epollevent_t *)ev_buf->data.ptr, _cur_nsec);
	ev_buf++;
    }
    
    size -= n;
    while (size && !skrb_mutex_empty(&el->tr_tree, &el->mutex)) {
	if (({node = skrb_mutex_min(&el->tr_tree, &el->mutex); node->key;}) > _cur_nsec)
	    break;
	ev = (epollevent_t *)node->data;
	skrb_mutex_delete(&el->tr_tree, &ev->tr_node, &el->mutex);
	ev->tr_node.key = 0;
	ev_buf->data.ptr = ev;
	ev_buf->events = EPOLLTIMEOUT;
	size--;
	ev_buf++;
    }
    return ev_buf - el->ev_buf;
}


int epoll_oneloop(epoll_t *el) {
    int n = 0;
    epollevent_t *et;
    struct epoll_event *ev_buf = el->ev_buf;
    
    if ((n = __epoll_wait(el)) < 0)
	return -1;
    while (ev_buf < el->ev_buf + n) {
	et = (epollevent_t *)ev_buf->data.ptr;
	et->happened = ev_buf->events;
	et->f(el, et);
	ev_buf++;
    }
    return 0;
}


int epoll_startloop(epoll_t *el) {
    el->stopping = false;
    while (!el->stopping)
	epoll_oneloop(el);
    return 0;
}

void epoll_stoploop(epoll_t *el) {
    el->stopping = true;
}
