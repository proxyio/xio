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

#ifndef _H_PROXYIO_EV_
#define _H_PROXYIO_EV_

#include <utils/list.h>
#include <utils/spinlock.h>
#include <utils/waitgroup.h>
#include <ev/eventpoll.h>

enum {
	EV_READ     =        0x01, /* ev_io detected read will not block */
	EV_WRITE    =        0x02, /* ev_io detected write will not block */
	EV_TIMER    =        0x04, /* timer timed out */
	EV_EXIT     =        0x08, /* ev_io exit event */

};

enum {
	EV_MAXEVENTS   =     100,  /* the max number of poll events */
};


	struct ev_timer;
typedef void (*ev_timer_hndl) (struct ev_timer *timer, int events /* EV_TIMER */);

struct ev_timer {
	struct list_head item;
	uint64_t timeout;
	ev_timer_hndl hndl;
};

/* initialize the timer by timeouts value and a hndl for processing the timeout events */
void ev_timer_init (struct ev_timer *w, uint64_t timeout, ev_timer_hndl hndl);

/* ev_timerset stores a list of timers and reports the next one to expire
   along with the time till it happens. */
struct ev_timerset {
	struct list_head head;
	uint64_t ftill;
};

void ev_timerset_init (struct ev_timerset *timerset);

/* terminate the timerset and trigger EV_EXIT event for each timer */
void ev_timerset_term (struct ev_timerset *timerset);

/* Add timer into timerset, timer must be initial by ev_timer_init () */
int ev_timerset_add (struct ev_timerset *timerset, struct ev_timer *timer);

/* Remove timer from timerset */
int ev_timerset_rm (struct ev_timerset *timerset, struct ev_timer *timer);

/* process all timers if it is timeouted */
int ev_timerset_timeout (struct ev_timerset *timerset);



enum {
	EV_ADD  =  0x00,   /* register the target fd on the eventpoll instance referred to by evp */
	EV_DEL  =  0x01,   /* remove the target fd from the eventpoll instance referred to by evp */
	EV_MOD  =  0x02,   /* change the event event associated with the target file descriptor fd. */
};

struct ev_fd;
struct ev_fdset;
typedef void (*ev_fd_hndl) (struct ev_fd *evfd, int events /* EV_READ|EV_WRITE */);

struct ev_task {
	struct list_head item;
	waitgroup_t *wg;
	int rc;
	int (*hndl) (struct ev_fdset *evfds, struct ev_fd *evfd);
};

struct ev_fd {
	struct list_head item;
	struct ev_task task;
	struct fdd fdd;
	int fd;
	int events;
	ev_fd_hndl hndl;
};

#define walk_ev_task_s(pos, tmp, head)					\
	walk_each_entry_s (pos, tmp, head, struct ev_fd, task.item)


/* initialize the ev_fd by file descriptor and a hndl for process watched events
   if happened */
static inline void ev_fd_init (struct ev_fd *evfd)
{
	INIT_LIST_HEAD (&evfd->item);
	evfd->task.wg = 0;
	evfd->task.rc = 0;
	INIT_LIST_HEAD (&evfd->task.item);
	fdd_init (&evfd->fdd);
}

struct ev_fdset {
	spin_t lock;                /* protect task_head fields */
	struct list_head task_head;
	struct list_head head;
	int fd_size;
	struct eventpoll eventpoll;
};

/* initialize the ev_fds head for storing a list of ev_fd and initialize the
   underlying eventpoll */
void ev_fdset_init (struct ev_fdset *evfds);

/* terminate ev_fdset, remove all fd from eventpool and then close it (eventpoll) */
void ev_fdset_term (struct ev_fdset *evfds);

int ev_fdset_ctl (struct ev_fdset *evfds, int op /* EV_ADD|EV_DEL|EV_MOD */,
		  struct ev_fd *evfd);

/* nolock version of ev_fdset_ctl, only called in ev_fd.hndl() */
int __ev_fdset_ctl (struct ev_fdset *evfds, int op /* EV_ADD|EV_DEL|EV_MOD */,
		    struct ev_fd *evfd);

/* process the underlying eventpoll and then process the happened fd events */
int ev_fdset_poll (struct ev_fdset *evfds, uint64_t timeout);




#endif
