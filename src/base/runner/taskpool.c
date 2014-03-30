#define __STDC_FORMAT_MACROS
#include <stdio.h>
#include <inttypes.h>
#include "os/memory.h"
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
	tp->tasks++;
	return 0;
    }
    return -1;
}

static int taskpool_push(taskpool_t *tp, thread_func func, void *data) {
    int rc = 0;
    mutex_lock(&tp->mutex);
    if ((rc = __taskpool_push(tp, func, data)) == 0)
	condition_broadcast(&tp->cond);
    mutex_unlock(&tp->mutex);
    return rc;
}

static struct __task *_taskpool_pop(taskpool_t *tp) {
    struct __task *te = NULL;
    
    if (!list_empty(&tp->task_head)) {
	te = list_first(&tp->task_head, struct __task, node);
	list_del(&te->node);
    }
    return te;
}

static struct __task *taskpool_pop(taskpool_t *tp) {
    struct __task *te = NULL;
    mutex_lock(&tp->mutex);
    if (!tp->stopping && !(te = _taskpool_pop(tp)))
	condition_wait(&tp->cond, &tp->mutex);
    mutex_unlock(&tp->mutex);
    return te;
}


int taskpool_run(taskpool_t *tp, thread_func func, void *data) {
    return taskpool_push(tp, func, data);
}


static inline int __taskpool_runner(void *data) {
    taskpool_t *tp = (taskpool_t *)data;
    struct __task *te = NULL;

    while (!tp->stopping) {
	if (!(te = taskpool_pop(tp)))
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
    mutex_lock(&tp->mutex);
    tp->stopping = true;
    condition_broadcast(&tp->cond);
    mutex_unlock(&tp->mutex);
    while (tpos < (tp->threads + tp->workers))
	thread_stop(*tpos++);
    return 0;
}
