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
#include <utils/atomic.h>
#include <utils/taskpool.h>

static void ev_sigfd_hndl (struct ev_fdset *evfds, struct ev_fd *evfd, int events);

void ev_sig_init (struct ev_sig *sig, ev_sig_hndl hndl)
{
	spin_init (&sig->lock);
	efd_init (&sig->efd);
	sig->hndl = hndl;
	ev_fd_init (&sig->evfd);
	sig->evfd.fd = sig->efd.r;
	sig->evfd.events = EV_READ;
	sig->evfd.hndl = ev_sigfd_hndl;
	bio_init (&sig->signals);
}

void ev_sig_term (struct ev_sig *sig)
{
	spin_destroy (&sig->lock);
	efd_destroy (&sig->efd);
	bio_destroy (&sig->signals);
}

void ev_signal (struct ev_sig *sig, int signo)
{
	int rc;

	spin_lock (&sig->lock);
	if (bio_size (&sig->signals) == 0)
		efd_signal (&sig->efd, 1);
	rc = bio_write (&sig->signals, (char *) &signo, sizeof (signo));
	BUG_ON (rc != sizeof (signo));
	spin_unlock (&sig->lock);
}

static int ev_get_signals (struct ev_sig *sig, int *sigset, int size)
{
	int rc;
	spin_lock (&sig->lock);
	rc = bio_read (&sig->signals, (char *) sigset, sizeof (int) * size);
	spin_unlock (&sig->lock);
	rc /= sizeof (int);
	return rc;
}

static void ev_unsignal (struct ev_sig *sig)
{
	int rc;
	int sigset[1024];

	spin_lock (&sig->lock);
	if (bio_empty (&sig->signals)) {
		while ((rc = efd_unsignal2 (&sig->efd, sigset,
					    NELEM (sigset, int))) > 0) {
		}
	} else
		efd_unsignal (&sig->efd);
	spin_unlock (&sig->lock);
}

static void ev_sigfd_hndl (struct ev_fdset *evfds, struct ev_fd *evfd,
    int events)
{
	int rc;
	int i;
	int sigset[1024];
	struct ev_sig *sig = cont_of (evfd, struct ev_sig, evfd);

	while ((rc = ev_get_signals (sig, sigset, 1)) > 0) {
		for (i = 0; i < rc; i++)
			sig->hndl (sig, sigset[i]);
	}
	ev_unsignal (sig);
}




void ev_fdset_init (struct ev_fdset *evfds)
{
	spin_init (&evfds->lock);
	INIT_LIST_HEAD (&evfds->task_head);
	evfds->fd_size = 0;
	eventpoll_init (&evfds->eventpoll);
	ev_mstats_init (&evfds->stats);
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

	if ((rc = eventpoll_ctl (&evfds->eventpoll, EV_ADD, fdd)) == 0)
		evfds->fd_size++;
	return rc;
}

int ev_fdset_rm (struct ev_fdset *evfds, struct ev_fd *evfd)
{
	int rc;

	if ((rc = eventpoll_ctl (&evfds->eventpoll, EV_DEL, &evfd->fdd)) == 0)
		evfds->fd_size--;
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



struct fd_async_task {
	struct ev_fd *owner;
	fds_ctl_op f;
	struct ev_task task;
};

static int fd_async_task_hndl (struct ev_loop *el, struct ev_task *ts)
{
	struct fd_async_task *ats = cont_of (ts, struct fd_async_task, task);
	return ats->f (&el->fdset, ats->owner);
}

static int ev_fdset_run_task (struct ev_fdset *evfds, struct ev_task *ts)
{
	int rc = 0;
	
	waitgroup_add (&ts->wg);
	spin_lock (&evfds->lock);
	list_add_tail (&ts->item, &evfds->task_head);
	spin_unlock (&evfds->lock);
	waitgroup_wait (&ts->wg);

	if (ts->rc < 0) {
		errno = -ts->rc;
		rc = -1;
	}
	return rc;
}
	

int ev_fdset_ctl (struct ev_fdset *evfds, int op, struct ev_fd *evfd)
{
	int rc;
	struct fd_async_task ats;
	struct ev_task *task = &ats.task;

	if (op >= NELEM (fds_ctl_vfptr, fds_ctl_op) || !fds_ctl_vfptr[op]) {
		errno = EINVAL;
		return -1;
	}
	ev_task_init (task);
	task->hndl = fd_async_task_hndl;

	ats.owner = evfd;
	ats.f = fds_ctl_vfptr [op];
	
	rc = ev_fdset_run_task (evfds, task);
	ev_task_destroy (task);
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


int ev_fdset_sighndl (struct ev_fdset *evfds, struct ev_sig *sig)
{
	return ev_fdset_ctl (evfds, EV_ADD, &sig->evfd);
}

int ev_fdset_unsighndl (struct ev_fdset *evfds, struct ev_sig *sig)
{
	return ev_fdset_ctl (evfds, EV_DEL, &sig->evfd);
}


int ev_fdset_poll (struct ev_fdset *evfds, uint64_t to)
{
	int rc;
	int i;
	struct ev_fd *evfd;
	struct ev_task *task;
	struct ev_task *tmp;
	struct fdd *fdds[EV_MAXEVENTS] = {};
	struct ev_loop *el;
	struct list_head task_head = LIST_HEAD_INITIALIZE (task_head);

	el = cont_of (evfds, struct ev_loop, fdset);
	spin_lock (&evfds->lock);
	list_splice (&evfds->task_head, &task_head);
	spin_unlock (&evfds->lock);

	walk_ev_task_s (task, tmp, &task_head) {
		list_del_init (&task->item);
		if ((task->rc = task->hndl (el, task)) < 0)
			task->rc = -errno;
		waitgroup_done (&task->wg);
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
static struct ev_loop ev_loops[EV_MAXPROCESSORS];

static spin_t ev_glock;
static struct list_head ev_tasks;



void ev_add_gtask (struct ev_task *ts)
{
	spin_lock (&ev_glock);
	list_add_tail (&ts->item, &ev_tasks);
	spin_unlock (&ev_glock);
}

static struct ev_task *ev_pop_gtask ()
{
	struct ev_task *ts = 0;

	spin_lock (&ev_glock);
	if (!list_empty (&ev_tasks)) {
		ts = list_first (&ev_tasks, struct ev_task, item);
		list_del_init (&ts->item);
	}
	spin_unlock (&ev_glock);
	return ts;
}


static int ev_hndl (void *hndl)
{
	struct ev_loop *el = (struct ev_loop *) hndl;
	struct ev_task *ts;

	ev_fdset_init (&el->fdset);
	waitgroup_done (&el->wg);

	while (!el->stopped) {
		ev_fdset_poll (&el->fdset, 1);
		if ((ts = ev_pop_gtask ()))
			ts->hndl (el, ts);
	}
	return 0;
}
	

void __ev_init (void)
{
	int i;
	char *env_max_cpus;
	struct ev_loop *el;

	spin_init (&ev_glock);
	INIT_LIST_HEAD (&ev_tasks);

#if defined _SC_NPROCESSORS_ONLN
	if ((ev_processors = sysconf (_SC_NPROCESSORS_ONLN)) < 1)
		ev_processors = 1;
#endif
	if ((env_max_cpus = getenv (ENV_PROXYIO_MAX_CPUS))) {
		if (0 == sscanf (env_max_cpus, "%d", &ev_processors))
			ev_processors = 1;
	}
	taskpool_init (&ev_pool, ev_processors);
	taskpool_start (&ev_pool);

	for (i = 0; i < ev_processors; i++) {
		el = &ev_loops[i];
		waitgroup_init (&el->wg);
		waitgroup_add (&el->wg);
		el->stopped = false;
		taskpool_run (&ev_pool, ev_hndl, el);
		waitgroup_wait (&el->wg);
	}
}

void __ev_exit (void)
{
	int i;

	for (i = 0; i < ev_processors; i++)
		ev_loops[i].stopped = true;
	taskpool_stop (&ev_pool);
	taskpool_destroy (&ev_pool);
	spin_destroy (&ev_glock);
}


struct ev_loop *ev_get_loop (int hash)
{
	return &ev_loops[hash % ev_processors];
}

struct ev_loop *ev_get_loop_lla ()
{
	int i;
	int chosed = 0;
	struct ev_loop *cur_el;
	struct ev_loop *chosed_el = &ev_loops[0];

	for (i = 1; i < ev_processors; i++) {
		cur_el = &ev_loops[i];
		if (cur_el->fdset.fd_size < chosed_el->fdset.fd_size)
			chosed_el = cur_el;
	}
	return chosed_el;
}
