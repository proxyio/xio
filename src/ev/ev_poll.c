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
}

void eventpoll_term (struct eventpoll *evp)
{
	struct fdd *_fdd;
	struct fdd *tmp;

	walk_fdd_s (_fdd, tmp, &evp->fds) {
		list_del_init (&_fdd->item);
	}
}

typedef int (*eventpoll_ctl_op) (struct eventpoll *evp, struct fdd *fdd);

static int eventpoll_add (struct eventpoll *evp, struct fdd *fdd)
{
	BUG_ON (!list_empty (&fdd->item));
	list_add_tail (&fdd->item, &evp->fds);
	return 0;
}

static int eventpoll_del (struct eventpoll *evp, struct fdd *fdd)
{
	BUG_ON (list_empty (&fdd->item));
	list_del_init (&fdd->item);
	return 0;
}

static int eventpoll_mod (struct eventpoll *evp, struct fdd *fdd)
{
	BUG_ON (list_empty (&fdd->item));
	return 0;
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
	struct pollfd fds[EV_MAXEVENTS] = {};
	struct list_head fd_head = LIST_HEAD_INITIALIZE (fd_head);
	struct ev_fdset *evfds;

	evfds = cont_of (evp, struct ev_fdset, eventpoll);
	if (max > EV_MAXEVENTS)
		max = EV_MAXEVENTS;
	walk_fdd_s (fdd, tmp, &evp->fds) {
		/* fd_size is the number of fds array */
		if (fd_size >= max)
			break;
		fds[fd_size].fd = fdd->fd;
		if (fdd->events & EV_READ)
			fds[fd_size].events |= POLLIN|POLLHUP|POLLERR;
		if (fdd->events & EV_WRITE)
			fds[fd_size].events |= POLLOUT;
		list_move (&fdd->item, &fd_head);
		fdds[fd_size++] = fdd;
	}
	list_splice (&fd_head, &evp->fds);

	if ((rc = poll (fds, fd_size, timeout)) <= 0) {
		ev_mstats_incr (&evfds->stats, ST_EV_NOTHG);
		return rc;
	}
	for (i = 0, rc = 0; i < fd_size; i++) {
		fdd = fdds[i];
		fdd->ready_events = 0;
		if (fds[i].revents) {
			if (fds[i].revents & (POLLIN|POLLHUP|POLLERR)) {
				fdd->ready_events |= EV_READ;
				ev_mstats_incr (&evfds->stats, ST_EV_IN);
			}
			if (fds[i].revents & (POLLOUT)) {
				fdd->ready_events |= EV_WRITE;
				ev_mstats_incr (&evfds->stats, ST_EV_OUT);
			}
			fdds[rc++] = fdd;
		}
	}
	mstats_base_emit (&evfds->stats.base, gettimeofms ());
	return rc;
}
