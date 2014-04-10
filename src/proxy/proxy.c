#include <ds/list.h>
#include <uuid/uuid.h>
#include "proxy.h"

#define PROXYIO_LISTENER
#define PROXYIO_CONNECTOR

typedef void (*event_handler) (struct pyfd *fd, uint32_t events);

struct pyio {
    int flags;
    struct upoll_tb *tb;
    struct list_head listener_head;
    struct list_head rcver_head;
    struct list_head snder_head;
    struct list_head unknown_head;
    ssmap_tree_t pyfd_rb_head;
};

#define list_for_each_pyfd(fd, nx, head)			\
    list_for_each_entry_safe(fd, nx, head, struct pyfd, link)

struct pyfd {
    struct upoll_entry event;
    int cd;
    int ty;
    uint32_t ok:1;
    event_handler *f;
    struct pyio *py;
    uint64_t mq_size;
    struct list_head mq;
    struct list_head link;
    uuid_t uuid;
    ssmap_node_t rb_link;
};

struct pyio *pyio_new() {
    struct pyio *py = (struct pyio *)mem_zalloc(sizeof(*py));
    if (py) {
	INIT_LIST_HEAD(&py->fd_head);
	INIT_LIST_HEAD(&py->listener_head);
	INIT_LIST_HEAD(&py->rcver_head);
	INIT_LIST_HEAD(&py->snder_head);
	INIT_LIST_HEAD(&py->unknown_head);
	ssmap_init(&py->pyfd_rb_head);
	py->tb = upoll_create();
    }
    return py;
}

int proxy_close(struct pyio *py) {
    if (!py) {
	errno = EINVAL;
	return -1;
    }
    upoll_close(py->tb);
    mem_free(py, sizeof(*py));
}

static struct pyfd pyfd_new() {
    struct pyfd *fd = (struct pyfd *)mem_zalloc(sizeof(*fd));
    if (fd) {
	INIT_LIST_HEAD(&fd->link);
    }
    return fd;
}

static void pyfd_free(struct pyfd *fd) {
    mem_free(fd, sizeof(*fd));
}


static inline int try_disable_eventout(struct pyfd *fd) {
    int rc;
    struct pyio *py = fd->py;

    if (fd->event.care & UPOLLOUT) {
	fd->event.care &= ~UPOLLOUT;
	BUG_ON((rc = upoll_mod(py->tb, &fd->event)) != 0);
	return rc;
    }
    return -1;
}

static inline int try_enable_eventout(struct pyfd *fd) {
    int rc;
    struct pyio *py = fd->py;

    if (!(fd->event.care & UPOLLOUT)) {
	fd->event.care |= UPOLLOUT;
	BUG_ON((rc = upoll_mod(py->tb, &fd->event)) != 0);
	return rc;
    }
    return -1;
}

static struct gsm *gsm_head_pop(struct pyfd *fd) {
    struct gsm *s = 0;

    if (!list_empty(&fd->mq)) {
	fd->mq_size--;
	s = list_first(&fd->mq, struct gsm, link);
	list_del_init(&s->link);
    } else if (list_empty(&fd->mq))
	try_disable_eventout(fd);
    return s;
}

static int gsm_head_push(struct pyfd *fd, struct gsm *s) {
    fd->mq_size++;
    list_add_tail(&s->link, &fd->mq);
    if (!list_empty(&fd->mq))
	try_enable_eventout(r);
    return 0;
}


/* Receive one request from frontend channel */
void rcver_recv(struct pyfd *fd) {
    int64_t now = rt_mstime();
    struct pyfd *go_fd;
    struct gsm *s;
    char *payload;
    struct pyio *py = fd->py;

    if (channel_recv(fd->cd, &payload) == 0) {
	if (list_empty(&py->snder_head) || !(s = gsm_new(payload))) {
	    channel_freemsg(payload);
	    return;
	}
	/* Drop the timeout massage */
	if (gsm_timeout(s, now) < 0) {
	    gsm_free(s);
	    return;
	}
	/* If massage has invalid checksum. set fd in bad status */
	if (gsm_validate(s) < 0) {
	    gsm_free(s);
	    fd->ok = false;
	    return;
	}
	tr_go_cost(s, now);

	/* Round robin algo, selete a go_fd */
	go_fd = list_first(&py->snder_head, struct pyfd, link);
	list_move_tail(&go_fd->link, &py->snder_head);

	gsm_head_push(go_fd, s);
    } else if (errno != EAGAIN) {
	/* bad things happened */
	fd->ok = false;
    }
}

/* Send one response to frontend channel */
void rcver_send(struct pyfd *fd) {
    int64_t now = rt_mstime();
    struct gsm *s;
    struct pyio *py = fd->py;

    if (!(s = gsm_head_pop(fd)))
	return;
    tr_shrink_and_back(s, now);
    if (channel_send(fd->cd, s->payload) < 0 && errno != EAGAIN)
	fd->ok = false;
}


/* Dispatch one request to backend channel */
void snder_recv(struct pyfd *fd) {
    int64_t now = rt_mstime();
    struct pyfd *back_fd;
    struct gsm *s;
    char *payload;
    struct pyio *py = fd->py;

    if (channel_recv(fd->cd, &payload) == 0) {
	if (list_empty(&py->rcver_head) || !(s = gsm_new(payload))) {
	    channel_freemsg(payload);
	    return;
	}
	/* Drop the timeout massage */
	if (gsm_timeout(s, now) < 0) {
	    gsm_free(s);
	    return;
	}
	/* If massage has invalid checksum. set fd in bad status */
	if (gsm_validate(s) < 0) {
	    gsm_free(s);
	    fd->ok = false;
	    return;
	}
	tr_back_cost(s, now);

	/* Round robin algo, selete a out connector */
	back_fd = list_first(&py->snder_head, struct pyfd, link);
	list_move_tail(&back_fd->link, &py->snder_head);

	gsm_head_push(back_fd, s);
    } else if (errno != EAGAIN) {
	/* bad things happened */
	fd->ok = false;
    }
}


/* Receive one response from backend channel */
void snder_send(struct pyfd *fd) {
    struct pyio *py = fd->py;
}


void rcver_event_handler(struct pyfd *fd, uint32_t events) {
    struct pyio *py = fd->py;

    if (fd->ok && (events & UPOLLIN))
	rcver_recv(fd);
    if (fd->ok && (events & UPOLLOUT))
	rcver_send(fd);
}

void snder_event_handler(struct pyfd *fd, uint32_t events) {
    struct pyio *py = fd->py;

    if (fd->ok && (events & UPOLLIN))
	snder_recv(fd);
    if (fd->ok && (events & UPOLLOUT))
	snder_send(fd);
}

void py_connector_rgs(struct pyfd *fd, uint32_t events) {
    struct hgr *g;
    char *payload;

    if (!(events & UPOLLIN))
	return;
    if (channel_recv(fd->cd, &payload) == 0) {
	/* If the unregister channel's first message invalid, set bad status */
	if (channel_msglen(payload) != sizeof(*g) ||
	    !((g = (struct hgr *)payload)->type & (PIO_RCVER|PIO_SNDER))) {
	    channel_freemsg(payload);
	    fd->ok = false;
	    return;
	}
	uuid_copy(fd->uuid, g->id);
    }
}

void py_connector_handler(struct pyfd *fd, uint32_t events) {
    struct pyio *py = fd->py;

    switch (fd->ty) {
    case PIO_RCVER:
	rcver_event_handler(fd, events);
	break;
    case PIO_SNDER:
	snder_event_handler(fd, events);
	break;
    default:
	/* Unregister channel */
	py_connector_rgs(fd, events);
	break;
    }
    /* If pyfd status bad. destroy it */
    if (!fd->ok) {
	/* do something here */
    }
}

void py_listener_handler(struct pyfd *fd, uint32_t events) {
    struct pyio *py = fd->py;
    int new_cd;
    struct pyfd *new_fd;
    
    BUG_ON(events & UPOLLOUT);
    if (events & UPOLLIN) {
	if ((new_cd = channel_accept(py->cd)) < 0)
	    return;
	if (!(new_fd = pyfd_new()))
	    return;
	new_fd->event.cd = new_cd;
	new_fd->event.care = UPOLLIN|UPOLLOUT|UPOLLERR;
	new_fd->f = py_connector_handler;
	new_fd->cd = new_cd;
	new_fd->py = py;
	list_add_tail(&new_fd->link, &py->unknown_head);
	BUG_ON(upoll_add(py->tb, UPOLL_ADD, &new_fd->event) != 0);
    }

    /* If listener pyfd status bad. destroy it and we should relisten */
    if (!fd->ok) {
	/* do something here */
    }
}



int proxy_bind(struct pyio *py, const char *sock) {
    py->flags |= PYIO_LISTENER;
    return 0;
}

int proxy_connect(struct pyio *py, const char *peer) {
    py->flags |= PYIO_CONNECTOR;
    return 0;
}

char *proxy_recv(struct pyio *py) {
    char *payload = NULL;

    if (py->flags == (PYIO_LISTENER|PYIO_CONNECTOR)) {
	errno = EINVAL;
	return NULL;
    }
    return payload;
}

int proxy_send(struct pyio *py, char *payload) {
    if (py->flags == (PYIO_LISTENER|PYIO_CONNECTOR)) {
	errno = EINVAL;
	return -1;
    }
    return 0;
}
