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

#include "ev_select.h"
#include <ev/ev_define.h>


void fdd_init (struct fdd *fdd, int fd, int events)
{
	INIT_LIST_HEAD (&fdd->item);
	fdd->fd = fd;
	fdd->events = events;
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


int eventpoll_ctl (struct eventpoll *evp, int op, struct fdd *fdd)
{
}

/* intialize timeval structure from timeout value */
static void timeval_init (struct timeval *tv, int timeout /* millisecond */)
{
	tv->tv_sec = timeout / 1000;
	tv->tv_usec = (timeout % 1000) * 1000;
}


int eventpoll_wait (struct eventpoll *evp, struct fdd *fdds, int max, int timeout)
{
	fd_set readfds;
	fd_set writefds;
	struct fdd *fdd;
	struct fdd *tmp;
	int rc;
	int i;
	int fd_size = 0;
	struct timeval tv;

	timeval_init (&tv, timeout);
	FD_ZERO (&readfds);
	FD_ZERO (&writefds);

	if (max > FD_SETSIZE)
		max = FD_SETSIZE;
	walk_fdd_s (fdd, tmp, &evp->fds) {
		/* fd_size is the number of fdds array */
		if (fd_size >= max)
			break;
		if (fdd->events & EV_READ)
			FD_SET (fdd->fd, &readfds);
		if (fdd->events & EV_WRITE)
			FD_SET (fdd->fd, &writefds);
		fdds[fd_size++] = *fdd;
	}
	if ((rc = select (fd_size, &readfds, &writefds, NULL, &tv)) < 0)
		return -1;
	for (i = 0, rc = 0; i < fd_size; i++) {
		fdd = &fdds[i];
		if (!FD_ISSET (fdd->fd, &readfds) && !FD_ISSET (fdd->fd, &writefds))
			continue;
		fdds[rc++] = fdds[i];
	}
	return rc;
}
