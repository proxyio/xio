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

#include <ev/ev.h>

void fdd_init (struct fdd *fdd)
{
	INIT_LIST_HEAD (&fdd->item);
}

void fdd_term (struct fdd *fdd)
{
}

void eventpoll_init (struct eventpoll *evp)
{
	INIT_LIST_HEAD (&evp->fds);
	BUG_ON ((evp->epfd = epoll_create (EV_MAXEVENTS)) < 0);
}

void eventpoll_term (struct eventpoll *evp)
{
	struct fdd *fdd;
	struct fdd *tmp;

	walk_fdd_s (fdd, tmp, &evp->fds) {
		list_del_init (&fdd->item);
	}
	close (evp->epfd);
}

typedef int (*eventpoll_ctl_op) (struct eventpoll *evp, struct fdd *fdd);

static void epoll_event_init (struct epoll_event *et, struct fdd *fdd)
{
	if (fdd->events & EV_READ)
		et->events |= EPOLLIN|EPOLLERR|EPOLLHUP|EPOLLRDHUP;
	if (fdd->events & EV_WRITE)
		et->events |= EPOLLOUT;
	et->data.ptr = fdd;
}


static int eventpoll_add (struct eventpoll *evp, struct fdd *fdd)
{
	int rc;
	struct epoll_event et = {};

	BUG_ON (!list_empty (&fdd->item));
	epoll_event_init (&et, fdd);
	if ((rc = epoll_ctl (evp->epfd, EPOLL_CTL_ADD, fdd->fd, &et)) == 0) {
		list_add_tail (&fdd->item, &evp->fds);
	}
	return rc;
}

static int eventpoll_del (struct eventpoll *evp, struct fdd *fdd)
{
	int rc;
	struct epoll_event et = {};

	BUG_ON (list_empty (&fdd->item));
	epoll_event_init (&et, fdd);
	if ((rc = epoll_ctl (evp->epfd, EPOLL_CTL_DEL, fdd->fd, &et)) == 0) {
		list_del_init (&fdd->item);
	}
	return rc;
}

static int eventpoll_mod (struct eventpoll *evp, struct fdd *fdd)
{
	int rc;
	struct epoll_event et = {};

	BUG_ON (list_empty (&fdd->item));
	epoll_event_init (&et, fdd);
	rc = epoll_ctl (evp->epfd, EPOLL_CTL_MOD, fdd->fd, &et);
	return rc;
}

static eventpoll_ctl_op evp_ctl_vfptr[] = {
	eventpoll_add,
	eventpoll_del,
	eventpoll_mod,
};

int eventpoll_ctl (struct eventpoll *evp, int op, struct fdd *fdd)
{
	int rc;
	if (op >= NELEM (evp_ctl_vfptr, eventpoll_ctl_op) || !evp_ctl_vfptr[op]) {
		errno = EINVAL;
		return -1;
	}
	rc = evp_ctl_vfptr[op] (evp, fdd);
	return rc;
}

int eventpoll_wait (struct eventpoll *evp, struct fdd **fdds, int max, int timeout)
{
	int rc;
	int i;
	struct fdd *fdd;
	struct fdd *tmp;
	int fd_size = 0;
	struct epoll_event *et;
	struct epoll_event fds[EV_MAXEVENTS] = {};

	if (max > EV_MAXEVENTS)
		max = EV_MAXEVENTS;
	if ((rc = epoll_wait (evp->epfd, fds, max, timeout)) <= 0)
		return rc;
	fd_size = rc;
	for (i = 0, rc = 0; i < fd_size; i++) {
		et = &fds[i];
		if (!et->events)
			continue;
		fdd = (struct fdd *) et->data.ptr;
		fdd->ready_events = 0;
		if (et->events & (EPOLLIN|EPOLLERR|EPOLLHUP|EPOLLRDHUP))
			fdd->ready_events |= EV_READ;
		if (et->events & EPOLLOUT)
			fdd->ready_events |= EV_WRITE;
		fdds[rc++] = fdd;
	}
	return rc;
}
