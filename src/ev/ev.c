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
#include <unistd.h>
#include <utils/taskpool.h>

void ev_fdset_init (struct ev_fdset *evfds)
{
	spin_init (&evfds->lock);
	INIT_LIST_HEAD (&evfds->head);
	INIT_LIST_HEAD (&evfds->task_head);
	evfds->fd_size = 0;
	eventpoll_init (&evfds->eventpoll);
}

void ev_fdset_term (struct ev_fdset *evfds)
{
	spin_destroy (&evfds->lock);
	eventpoll_term (&evfds->eventpoll);
}

typedef int (*fds_ctl_op) (struct ev_fdset *evfds, struct ev_fd *evfd);

int ev_fdset_add (struct ev_fdset *evfds, struct ev_fd *evfd)
{
	int rc;
	struct fdd *fdd = &evfd->fdd;

	fdd->fd = evfd->fd;
	fdd->events = evfd->events;

	if ((rc = eventpoll_ctl (&evfds->eventpoll, EV_ADD, fdd)) == 0) {
		BUG_ON (!list_empty (&evfd->item));
		evfds->fd_size++;
		list_add_tail (&evfd->item, &evfds->head);
	}
	return rc;
}

int ev_fdset_rm (struct ev_fdset *evfds, struct ev_fd *evfd)
{
	int rc;

	if ((rc = eventpoll_ctl (&evfds->eventpoll, EV_DEL, &evfd->fdd)) == 0) {
		BUG_ON (list_empty (&evfd->item));
		evfds->fd_size--;
		list_del_init (&evfd->item);
	}
	return rc;
}

int ev_fdset_mod (struct ev_fdset *evfds, struct ev_fd *evfd)
{
	int rc;
	struct fdd *fdd = &evfd->fdd;
	int fd = fdd->fd;
	int events = fdd->events;

	fdd->fd = evfd->fd;
	fdd->events = evfd->events;
	if ((rc = eventpoll_ctl (&evfds->eventpoll, EV_MOD, fdd)) != 0) {
		/* restore the origin fdd setting */
		fdd->fd = fd;
		fdd->events = events;
	}
	return rc;
}

static fds_ctl_op fds_ctl_vfptr [] = {
	ev_fdset_add,
	ev_fdset_rm,
	ev_fdset_mod,
};

int ev_fdset_ctl (struct ev_fdset *evfds, int op, struct ev_fd *evfd)
{
	int rc = 0;
	waitgroup_t wg;
	struct ev_task *task = &evfd->task;
	
	if (op >= NELEM (fds_ctl_vfptr, fds_ctl_op) || !fds_ctl_vfptr[op]) {
		errno = EINVAL;
		return -1;
	}
	waitgroup_init (&wg);
	task->hndl = fds_ctl_vfptr[op];
	task->wg = &wg;
	waitgroup_add (&wg);

	spin_lock (&evfds->lock);
	list_add_tail (&task->item, &evfds->task_head);
	spin_unlock (&evfds->lock);
	waitgroup_wait (&wg);

	waitgroup_term (&wg);
	if (task->rc < 0) {
		errno = -task->rc;
		rc = -1;
	}
	return rc;
}

int __ev_fdset_ctl (struct ev_fdset *evfds, int op, struct ev_fd *evfd)
{
	int rc;

	if (op >= NELEM (fds_ctl_vfptr, fds_ctl_op) || !fds_ctl_vfptr[op]) {
		errno = EINVAL;
		return -1;
	}
	rc = fds_ctl_vfptr[op] (evfds, evfd);
	return rc;
}

int ev_fdset_poll (struct ev_fdset *evfds, uint64_t to)
{
	int rc;
	int i;
	struct ev_fd *evfd;
	struct ev_fd *tmp;
	struct ev_task *task;
	struct fdd *fdds[EV_MAXEVENTS] = {};
	struct list_head task_head = LIST_HEAD_INITIALIZE (task_head);

	spin_lock (&evfds->lock);
	list_splice (&evfds->task_head, &task_head);
	spin_unlock (&evfds->lock);

	walk_ev_task_s (evfd, tmp, &task_head) {
		list_del_init (&evfd->item);
		task = &evfd->task;
		if ((task->rc = task->hndl (evfds, evfd)) < 0)
			task->rc = -errno;
		if (task->wg)
			waitgroup_done (task->wg);
	}
	if ((rc = eventpoll_wait (&evfds->eventpoll, fdds, EV_MAXEVENTS, to)) <= 0)
		return rc;
	for (i = 0; i < rc; i++) {
		evfd = cont_of (fdds[i], struct ev_fd, fdd);
		evfd->hndl (evfds, evfd, fdds[i]->ready_events);
	}
	return 0;
}

static int ev_processors = 1;    /* The number of processors currently online */
static struct taskpool ev_pool;

struct ev_loop ev_loops[EV_MAXPROCESSORS];

static int ev_hndl (void *hndl)
{
	struct ev_loop *ev_loop = (struct ev_loop *) hndl;
	ev_fdset_init (&ev_loop->fdset);
	waitgroup_done (&ev_loop->wg);

	while (!ev_loop->stopped)
		ev_fdset_poll (&ev_loop->fdset, 1);
	return 0;
}
	

void __attribute__ ((constructor)) __ev_init (void)
{
	int i;
	struct ev_loop *ev_loop;

#if defined _SC_NPROCESSORS_ONLN
	if ((ev_processors = sysconf (_SC_NPROCESSORS_ONLN)) < 1)
		ev_processors = 1;
#endif
	taskpool_init (&ev_pool, ev_processors);
	taskpool_start (&ev_pool);

	for (i = 0; i < ev_processors; i++) {
		ev_loop = &ev_loops[i];
		waitgroup_init (&ev_loop->wg);
		waitgroup_add (&ev_loop->wg);
		ev_loop->stopped = false;
		taskpool_run (&ev_pool, ev_hndl, ev_loop);
		waitgroup_wait (&ev_loop->wg);
	}
}

void __attribute__ ((destructor)) __ev_exit (void)
{
	int i;

	for (i = 0; i < ev_processors; i++)
		ev_loops[i].stopped = true;
	taskpool_stop (&ev_pool);
}


struct ev_loop *ev_get_loop (int hash)
{
	return &ev_loops[hash % ev_processors];
}
