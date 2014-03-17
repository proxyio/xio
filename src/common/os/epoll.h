#ifndef _HPIO_EPOLLER_
#define _HPIO_EPOLLER_

#include <unistd.h>
#include <sys/epoll.h>
#include "memory.h"
#include "ds/list.h"
#include "ds/skrb.h"
#include "sync/mutex.h"


#define EPOLLTIMEOUT (1 << 28)
#ifndef EPOLLRDHUP
#define EPOLLRDHUP 0x2000
#endif

struct epollevent;
struct epoll;
typedef int (*event_handler) (struct epoll *el, struct epollevent *et);

typedef struct epollevent {
    event_handler f;
    void *data;
    int fd;
    int64_t to_nsec;
    uint32_t events;
    uint32_t happened;
    skrb_node_t tr_node;
    struct list_head el_link;
} epollevent_t;

#define list_for_each_et_safe(pos, tmp, head)				\
    list_for_each_entry_safe(pos, tmp, head, epollevent_t, el_link)


typedef struct epoll {
    int stopping;
    int efd, max_io_events, event_size;
    int64_t max_to;
    skrb_t tr_tree;
    mutex_t mutex;
    struct list_head link;
    struct epoll_event *ev_buf;
} epoll_t;

#define list_for_each_el_safe(pos, tmp, head)			\
    list_for_each_entry_safe(pos, tmp, head, epoll_t, link)


static inline epollevent_t *epollevent_new() {
    epollevent_t *ev = (epollevent_t *)mem_zalloc(sizeof(*ev));
    return ev;
}

static inline epoll_t *epoll_new() {
    epoll_t *el = (epoll_t *)mem_zalloc(sizeof(*el));
    return el;
}

static inline int epoll_init(epoll_t *el, int size, int max_io_events, int max_to) {
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

static inline int epoll_destroy(epoll_t *el) {
    epollevent_t *ev = NULL;
    mutex_destroy(&el->mutex);
    while (!skrb_empty(&el->tr_tree)) {
	ev = (epollevent_t *)(skrb_min(&el->tr_tree))->data;
	skrb_delete(&el->tr_tree, &ev->tr_node);
    }
    if (el->efd > 0)
	close(el->efd);
    if (el->ev_buf)
	mem_free(el->ev_buf, sizeof(*el->ev_buf) * el->max_io_events);
    return 0;
}

int epoll_add(epoll_t *el, epollevent_t *ev);
int epoll_del(epoll_t *el, epollevent_t *ev);
int epoll_mod(epoll_t *el, epollevent_t *ev);
int epoll_oneloop(epoll_t *el);
int epoll_startloop(epoll_t *el);
void epoll_stoploop(epoll_t *el);


#endif
