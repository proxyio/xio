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

#ifndef _H_PROXYIO_TASK_RUNNER_
#define _H_PROXYIO_TASK_RUNNER_

#include <utils/list.h>
#include <utils/mutex.h>
#include <utils/spinlock.h>
#include <utils/eventloop.h>
#include <utils/efd.h>

struct task_ent {
	void (*f) (struct task_ent *ts);
	struct list_head link;
};

#define walk_task_s(ts, tmp, head)				\
    walk_each_entry_s(ts, tmp, head, struct task_ent, link)

struct task_runner {
#if defined HAVE_DEBUG
	mutex_t lock;
#else
	spin_t lock;
#endif

	/* Backend eventloop for cpu_worker. */
	eloop_t el;

	ev_t efd_et;
	struct efd efd;

	/* Waiting for closed sock will be attached here */
	struct list_head shutdown_socks;
};

int task_runner_alloc();
int task_runner_choosed (int fd);
void task_runner_free (int cpu_no);
struct task_runner *get_task_runner (int cpu_no);

#if defined HAVE_DEBUG

static inline void task_runner_lock_init (struct task_runner *cpu)
{
	mutex_init (&cpu->lock);
}

static inline void task_runner_lock_destroy (struct task_runner *cpu)
{
	mutex_destroy (&cpu->lock);
}

static inline void task_runner_relock (struct task_runner *cpu)
{
	mutex_relock (&cpu->lock);
}

static inline void task_runner_lock (struct task_runner *cpu)
{
	mutex_lock (&cpu->lock);
}

static inline void task_runner_unlock (struct task_runner *cpu)
{
	mutex_unlock (&cpu->lock);
}

#else

static inline void task_runner_lock_init (struct task_runner *cpu)
{
	spin_init (&cpu->lock);
}

static inline void task_runner_lock_destroy (struct task_runner *cpu)
{
	spin_destroy (&cpu->lock);
}

static inline void task_runner_relock (struct task_runner *cpu)
{
	spin_relock (&cpu->lock);
}

static inline void task_runner_lock (struct task_runner *cpu)
{
	spin_lock (&cpu->lock);
}

static inline void task_runner_unlock (struct task_runner *cpu)
{
	spin_unlock (&cpu->lock);
}

#endif

#endif
