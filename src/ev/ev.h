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
#include <utils/bufio.h>
#include <utils/spinlock.h>
#include <utils/waitgroup.h>
#include <utils/efd.h>
#include <ev/eventpoll.h>
#include "ev_stats.h"

enum {
	EV_READ     =        0x01, /* ev_io detected read will not block */
	EV_WRITE    =        0x02, /* ev_io detected write will not block */
	EV_TIMER    =        0x04, /* timer timed out */
	EV_EXIT     =        0x08, /* ev_io exit event */

};

/* register/remove/update ev_hndl, such as ev_timer or ev_fd */
enum {
	EV_ADD  =  0x00,
	EV_DEL  =  0x01,
	EV_MOD  =  0x02,
};

enum {
	EV_MAXEVENTS      =  100,  /* the max number of poll events */
	EV_MAXPROCESSORS  =  32,   /* the number of processors currently configured */
};


struct ev_timer;
struct ev_timerset;
typedef void (*ev_timer_hndl) (struct ev_timerset *evts, struct ev_timer *timer,
    int events /* EV_TIMER */);

struct ev_timer {
	struct list_head item;
	uint64_t timeout;
	ev_timer_hndl hndl;
};

/* initialize the timer by timeouts value and a hndl for processing the
   timeout events */
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

/* EV_ADD: Add timer into timerset, timer must be initial by ev_timer_init ()
   EV_DEL: Remove timer from timerset */
int ev_timerset_ctl (struct ev_timerset *timerset, int op /* EV_ADD|EV_DEL */,
		     struct ev_timer *timer);

/* process all timers if it is timeouted */
int ev_timerset_timeout (struct ev_timerset *timerset);



struct ev_loop;
struct ev_task;
typedef int (*ev_task_hndl) (struct ev_loop *el, struct ev_task *ts);

struct ev_task {
	struct list_head item;
	waitgroup_t wg;
	int rc;
	ev_task_hndl hndl;
};

#define walk_ev_task_s(pos, tmp, head)					\
	walk_each_entry_s (pos, tmp, head, struct ev_task, item)

static inline void ev_task_init (struct ev_task *ts)
{
	waitgroup_init (&ts->wg);
	ts->rc = 0;
	ts->hndl = 0;
	INIT_LIST_HEAD (&ts->item);
}

static inline void ev_task_destroy (struct ev_task *ts)
{
	waitgroup_destroy (&ts->wg);
}

static inline int ev_task_wait (struct ev_task *ts)
{
	waitgroup_wait (&ts->wg);
	return ts->rc;
}

void ev_add_gtask (struct ev_task *ts);



struct ev_fd;
struct ev_fdset;
typedef void (*ev_fd_hndl) (struct ev_fdset *evfds, struct ev_fd *evfd,
    int events /* EV_READ|EV_WRITE */);

struct ev_fd {
	struct fdd fdd;
	int fd;
	int events;
	ev_fd_hndl hndl;
};

/* initialize the ev_fd by file descriptor and a hndl for process watched events
   if happened */
static inline void ev_fd_init (struct ev_fd *evfd)
{
	fdd_init (&evfd->fdd);
}



struct ev_sig;
typedef void (*ev_sig_hndl) (struct ev_sig *sig, int signo);

struct ev_sig {
	spin_t lock;
	struct efd efd;
	struct ev_fd evfd;
	ev_sig_hndl hndl;
	struct bio signals;
};

void ev_sig_init (struct ev_sig *sig, ev_sig_hndl hndl);
void ev_sig_term (struct ev_sig *sig);
void ev_signal (struct ev_sig *sig, int signo);


struct ev_fdset {
	spin_t lock;                /* protect task_head fields */
	struct list_head task_head;
	int fd_size;
	struct eventpoll eventpoll;
	struct ev_mstats stats;
};

/* initialize the ev_fds head for storing a list of ev_fd and initialize the
   underlying eventpoll */
void ev_fdset_init (struct ev_fdset *evfds);

/* terminate ev_fdset, remove all fd from eventpool and then close it (eventpoll) */
void ev_fdset_term (struct ev_fdset *evfds);

int ev_fdset_ctl (struct ev_fdset *evfds, int op /* EV_ADD|EV_DEL|EV_MOD */,
		  struct ev_fd *evfd);

/* register the signal hndl into ev_fdset */
int ev_fdset_sighndl (struct ev_fdset *evfds, struct ev_sig *sig);

/* unregister the signal hndl from ev_fdset */
int ev_fdset_unsighndl (struct ev_fdset *evfds, struct ev_sig *sig);

/* nolock version of ev_fdset_ctl, only called in ev_fd's hndl */
int __ev_fdset_ctl (struct ev_fdset *evfds, int op /* EV_ADD|EV_DEL|EV_MOD */,
		    struct ev_fd *evfd);

/* process the underlying eventpoll and then process the happened fd events */
int ev_fdset_poll (struct ev_fdset *evfds, uint64_t timeout);











struct ev_loop {
	waitgroup_t wg;
	int stopped;
	struct ev_timerset timerset;
	struct ev_fdset fdset;
};

/* get one ev_loop from the backend ev_loops pool, hash is a hint to ev_kernel
   how to chosed ev_loop, the same hash mapped into the same ev_loop */
struct ev_loop *ev_get_loop (int hash);





#endif
