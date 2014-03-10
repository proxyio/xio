#ifndef _HPIO_THREAD_
#define _HPIO_THREAD_

#include <pthread.h>
#include <sys/syscall.h>
#include "base.h"
#include "os/malloc.h"

#define gettid() syscall(__NR_gettid)

typedef int (*thread_func)(void *);
typedef struct thread {
    pid_t krid;
    pthread_t tid;
    thread_func f;
    void *args;
    int res;
} thread_t;

static inline thread_t *thread_new() {
    thread_t *tt = (thread_t *)mem_zalloc(sizeof(*tt));
    return tt;
}

int thread_start(thread_t *tt, thread_func f, void *args);
int thread_stop(thread_t *tt);



#endif
