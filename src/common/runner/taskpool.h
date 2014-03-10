#ifndef _HPIO_TASKPOOL_
#define _HPIO_TASKPOOL_

#include "base.h"
#include "ds/list.h"
#include "sync/mutex.h"
#include "sync/condition.h"
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

static inline taskpool_t *taskpool_new() {
    taskpool_t *tp = (taskpool_t *)mem_zalloc(sizeof(*tp));
    return tp;
}

static inline void taskpool_free(taskpool_t *tp) {
    free(tp);
}

static inline int taskpool_init(taskpool_t *tp, int w) {
    int i;
    thread_t *tt = NULL, **tpos = NULL;

    tp->stopping = true;
    tp->tasks = 0;
    tp->workers = w;
    if (!(tp->threads = (thread_t **)mem_zalloc(sizeof(thread_t *) * w)))
	return -1;
    mutex_init(&tp->mutex);
    condition_init(&tp->cond);
    INIT_LIST_HEAD(&tp->task_head);
    if (tp->workers < 0)
	tp->workers = 1;
    tpos = tp->threads;
    for (i = 0; i < tp->workers; i++) {
	if ((tt = thread_new()) != NULL)
	    *tpos++ = tt;
    }
    if ((tp->workers = tpos - tp->threads) <= 0) {
	mem_free(tp->threads, sizeof(*tp->threads) * tp->workers);
	return -1;
    }
    return 0;
}

static inline int taskpool_destroy(taskpool_t *tp) {
    thread_t **tpos = tp->threads;

    while (tpos < (tp->threads + tp->workers))
	free(*tpos++);
    mem_free(tp->threads, tp->workers * sizeof(*tp->threads));
    mutex_destroy(&tp->mutex);
    condition_destroy(&tp->cond);
    ZERO(*tp);
    return 0;
}

int taskpool_run(taskpool_t *tp, thread_func tf, void *data);
int taskpool_start(taskpool_t *tp);
int taskpool_stop(taskpool_t *tp);




#endif   // _H_TASK_POOL_
