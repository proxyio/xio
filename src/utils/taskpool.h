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

#ifndef _H_PROXYIO_TASKPOOL_
#define _H_PROXYIO_TASKPOOL_

#include "base.h"
#include "list.h"
#include "mutex.h"
#include "condition.h"
#include "thread.h"

typedef struct taskpool {
	int stopping;
	int tasks;
	int workers;
	mutex_t mutex;
	condition_t cond;
	struct list_head task_head;
	thread_t **threads;
} taskpool_t;

static inline taskpool_t *taskpool_new()
{
	taskpool_t *tp = TNEW (taskpool_t);
	return tp;
}

static inline void taskpool_free (taskpool_t *tp)
{
	free (tp);
}

static inline int taskpool_init (taskpool_t *tp, int w)
{
	int i;
	thread_t *tt = NULL, **tpos = NULL;

	tp->stopping = true;
	tp->tasks = 0;
	tp->workers = w;
	if (! (tp->threads = NTNEW (thread_t *, w) ) )
		return -1;
	mutex_init (&tp->mutex);
	condition_init (&tp->cond);
	INIT_LIST_HEAD (&tp->task_head);
	if (tp->workers < 0)
		tp->workers = 1;
	tpos = tp->threads;
	for (i = 0; i < tp->workers; i++) {
		if ( (tt = thread_new() ) != NULL)
			*tpos++ = tt;
	}
	if ( (tp->workers = tpos - tp->threads) <= 0) {
		mem_free (tp->threads, sizeof (*tp->threads) * tp->workers);
		return -1;
	}
	return 0;
}

static inline int taskpool_destroy (taskpool_t *tp)
{
	thread_t **tpos = tp->threads;

	while (tpos < (tp->threads + tp->workers) )
		free (*tpos++);
	mem_free (tp->threads, tp->workers * sizeof (*tp->threads) );
	mutex_destroy (&tp->mutex);
	condition_destroy (&tp->cond);
	ZERO (*tp);
	return 0;
}

int taskpool_run (taskpool_t *tp, thread_func tf, void *data);
int taskpool_start (taskpool_t *tp);
int taskpool_stop (taskpool_t *tp);




#endif   // _H_PROXYIO_TASK_POOL_
