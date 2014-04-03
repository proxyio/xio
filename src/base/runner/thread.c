#include "thread.h"

static void *thread_routine (void *arg_)
{
    thread_t *self = (thread_t *) arg_;
    self->krid = gettid();
    self->res = self->f(self->args);
    return NULL;
}

int thread_start(thread_t *tt, thread_func f, void *args) {
    tt->f = f;
    tt->args = args;
    return pthread_create(&tt->tid, NULL, thread_routine, tt);
}

int thread_stop(thread_t *tt) {
    void *status = NULL;

    pthread_join (tt->tid, &status);
    return tt->res;
}
