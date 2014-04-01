#ifndef _HPIO_EVENTLOOP_
#define _HPIO_EVENTLOOP_

#include <unistd.h>
#include <sys/epoll.h>
#include "alloc.h"
#include "ds/list.h"
#include "ds/skrb.h"
#include "sync/mutex.h"


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
    skrb_node_t tr_node;
    struct list_head el_link;
} ev_t;

#define list_for_each_et_safe(pos, tmp, head)				\
    list_for_each_entry_safe(pos, tmp, head, ev_t, el_link)

typedef struct eloop {
    int stopping;
    int efd, max_io_events, event_size;
    int64_t max_to;
    skrb_t tr_tree;
    mutex_t mutex;
    struct list_head link;
    struct epoll_event *ev_buf;
} eloop_t;

#define list_for_each_el_safe(pos, tmp, head)			\
    list_for_each_entry_safe(pos, tmp, head, eloop_t, link)


static inline ev_t *epollevent_new() {
    ev_t *ev = (ev_t *)mem_zalloc(sizeof(*ev));
    return ev;
}

static inline eloop_t *epoll_new() {
    eloop_t *el = (eloop_t *)mem_zalloc(sizeof(*el));
    return el;
}

static inline int epoll_init(eloop_t *el, int size, int max_io_events, int max_to) {
    if ((el->efd = epoll_create(size)) < 0)
	return -1;
    if (!(el->ev_buf =
	  (struct epoll_event *)mem_zalloc(sizeof(*el->ev_buf) * max_io_events))) {
	close(el->efd);
	return -1;
    }
    el->max_to = max_to;
    skrb_init(&el->tr_tree);
    mutex_init(&el->mutex);
    el->max_io_events = max_io_events;
    return 0;
}

static inline int epoll_destroy(eloop_t *el) {
    ev_t *ev = NULL;
    mutex_destroy(&el->mutex);
    while (!skrb_empty(&el->tr_tree)) {
	ev = (ev_t *)(skrb_min(&el->tr_tree))->data;
	skrb_delete(&el->tr_tree, &ev->tr_node);
    }
    if (el->efd > 0)
	close(el->efd);
    if (el->ev_buf)
	mem_free(el->ev_buf, sizeof(*el->ev_buf) * el->max_io_events);
    return 0;
}

int epoll_add(eloop_t *el, ev_t *ev);
int epoll_del(eloop_t *el, ev_t *ev);
int epoll_mod(eloop_t *el, ev_t *ev);
int epoll_oneloop(eloop_t *el);
int epoll_startloop(eloop_t *el);
void epoll_stoploop(eloop_t *el);


#endif
