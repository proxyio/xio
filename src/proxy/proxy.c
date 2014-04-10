#include <ds/list.h>
#include "proxy.h"

#define PROXYIO_LISTENER
#define PROXYIO_CONNECTOR

typedef void (*event_handler) (struct py_fd *fd, uint32_t events);

struct proxyio {
    int flags;
    struct upoll_tb *tb;
    struct list_head listener_head;
    struct list_head rcver_head;
    struct list_head snder_head;
    struct list_head unknown_head;
};

struct py_fd {
    struct upoll_event event;
    int cd;
    int flags;
    event_handler *f;
    struct proxyio *py;
    struct list_head link;
};

struct proxyio *proxyio_new() {
    struct proxyio *py = (struct proxyio *)mem_zalloc(sizeof(*py));
    if (py) {
	INIT_LIST_HEAD(&py->fd_head);
	INIT_LIST_HEAD(&py->listener_head);
	INIT_LIST_HEAD(&py->rcver_head);
	INIT_LIST_HEAD(&py->snder_head);
	INIT_LIST_HEAD(&py->unknown_head);
	py->tb = upoll_create();
    }
    return py;
}

int proxy_close(struct proxyio *py) {
    if (!py) {
	errno = EINVAL;
	return -1;
    }
    upoll_close(py->tb);
    mem_free(py, sizeof(*py));
}

static struct py_fd py_fd_new() {
    struct py_fd *fd = (struct py_fd *)mem_zalloc(sizeof(*fd));
    if (fd) {
	INIT_LIST_HEAD(&fd->link);
    }
    return fd;
}

static void py_fd_free(struct py_fd *fd) {
    mem_free(fd, sizeof(*fd));
}


void py_connector_handler(struct py_fd *fd, uint32_t events) {
    struct proxyio *py = fd->py;

    if (evnets & UPOLLIN) {

    }
    if (evnets & UPOLLOUT) {

    }
    if (evnets & UPOLLERR) {
    }
}

void py_listener_handler(struct py_fd *fd, uint32_t events) {
    struct proxyio *py = fd->py;
    int new_cd;
    struct py_fd *new_fd;
    
    BUG_ON(events & UPOLLOUT);
    if (events & UPOLLIN) {
	if ((new_cd = channel_accept(py->cd)) < 0)
	    return;
	if (!(new_fd = py_fd_new()))
	    return;
	new_fd->event.cd = new_cd;
	new_fd->event.care = UPOLLIN|UPOLLOUT|UPOLLERR;
	new_fd->f = py_connector_handler;
	new_fd->cd = new_cd;
	new_fd->py = py;
	list_add_tail(&new_fd->link, &py->unknown_head);
	BUG_ON(upoll_add(py->tb, UPOLL_ADD, &new_fd->event) != 0);
    }
}



int proxy_bind(struct proxyio *py, const char *sock) {
    py->flags |= PROXYIO_LISTENER;
    return 0;
}

int proxy_connect(struct proxyio *py, const char *peer) {
    py->flags |= PROXYIO_CONNECTOR;
    return 0;
}

char *proxy_recv(struct proxyio *py) {
    char *payload = NULL;

    if (py->flags == (PROXYIO_LISTENER|PROXYIO_CONNECTOR)) {
	errno = EINVAL;
	return NULL;
    }
    return payload;
}

int proxy_send(struct proxyio *py, char *payload) {
    if (py->flags == (PROXYIO_LISTENER|PROXYIO_CONNECTOR)) {
	errno = EINVAL;
	return -1;
    }
    return 0;
}
