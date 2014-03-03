#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include "os/malloc.h"
#include "taskpool.h"

struct __task {
    thread_func f;
    void *data;
    struct list_head node;
};

static inline struct __task *__task_new() {
    struct __task *te = (struct __task *)mem_zalloc(sizeof(*te));
    return te;
}

static int __taskpool_push(taskpool_t *tp, thread_func func, void *data) {
    struct __task *te = NULL;

    if ((te = __task_new()) != NULL) {
	te->f = func;
	te->data = data;
	list_add_tail(&te->node, &tp->task_head);
	return ++tp->tasks;
    }
    return -1;
}

static inline void __taskpool_lock(taskpool_t *tp) {
    mutex_lock(&tp->mutex);
}

static inline void __taskpool_unlock(taskpool_t *tp) {
    mutex_unlock(&tp->mutex);
}

static inline void __taskpool_wait(taskpool_t *tp) {
    condition_wait(&tp->cond, &tp->mutex);
}

static inline void __taskpool_wakeup(taskpool_t *tp) {
    condition_broadcast(&tp->cond);
}


static int _taskpool_push(taskpool_t *tp, thread_func func, void *data) {
    int ret = 0, wakeup = false;
    __taskpool_lock(tp);
    if ((ret = __taskpool_push(tp, func, data)) == 0)
	wakeup = true;
    __taskpool_unlock(tp);
    if (wakeup)
	__taskpool_wakeup(tp);
    return ret;
}

static struct __task *__taskpool_pop(taskpool_t *tp) {
    struct __task *te = NULL;
    
    if (!list_empty(&tp->task_head)) {
	te = list_first(&tp->task_head, struct __task, node);
	list_del(&te->node);
    }
    return te;
}

static struct __task *_taskpool_pop(taskpool_t *tp) {
    struct __task *te = NULL;
    __taskpool_lock(tp);
    if (!(te = __taskpool_pop(tp)))
	__taskpool_wait(tp);
    __taskpool_unlock(tp);
    return te;
}


int taskpool_run(taskpool_t *tp, thread_func func, void *data) {
    return _taskpool_push(tp, func, data);
}


static inline int __taskpool_runner(void *data) {
    taskpool_t *tp = (taskpool_t *)data;
    struct __task *te = NULL;

    while (!tp->stopping) {
	if (!(te = _taskpool_pop(tp)))
	    continue;
	te->f(te->data);
	mem_free(te, sizeof(*te));
    }
    return 0;
}

int taskpool_start(taskpool_t *tp) {
    thread_t **tpos = tp->threads;

    if (!tp->stopping)
	return -1;
    tp->stopping = false;
    while (tpos < (tp->threads + tp->workers))
	thread_start(*tpos++, __taskpool_runner, tp);
    return 0;
}

int taskpool_stop(taskpool_t *tp) {
    thread_t **tpos = tp->threads;

    if (tp->stopping)
	return -1;
    tp->stopping = true;
    __taskpool_wakeup(tp);
    while (tpos < (tp->threads + tp->workers))
	thread_stop(*tpos++);
    return 0;
}
