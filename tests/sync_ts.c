#include <utils/thread.h>
#include <utils/waitgroup.h>

static int test_waitgroup_single_thread()
{
    waitgroup_t wg = {};

    waitgroup_init(&wg);
    waitgroup_add(&wg);
    waitgroup_done(&wg);
    waitgroup_wait(&wg);
    waitgroup_adds(&wg, 2);
    waitgroup_dones(&wg, 2);
    waitgroup_wait(&wg);
    waitgroup_adds(&wg, 2);
    waitgroup_done(&wg);
    waitgroup_done(&wg);
    waitgroup_wait(&wg);
    waitgroup_destroy(&wg);
    return 0;
}


static int task_func(void *data)
{
    int i = 0;
    waitgroup_t *wg = (waitgroup_t *)data;
    for (i = 0; i < 100; i++)
        waitgroup_done(wg);
    return 0;
}

static int test_waitgroup_multi_threads()
{
    waitgroup_t wg = {};
    int i, nthreads = 0;
    thread_t *tt = NULL;
    thread_t *workers[20];

    nthreads = rand() % 20;
    for (i = 0; i < nthreads; i++) {
        BUG_ON(!(tt = thread_new()));
        workers[i] = tt;
    }
    waitgroup_adds(&wg, nthreads * 100);
    for (i = 0; i != nthreads; i++) {
        tt = workers[i];
        thread_start(tt, task_func, &wg);
    }
    waitgroup_wait(&wg);
    for (i = 0; i != nthreads; i++) {
        tt = workers[i];
        thread_stop(tt);
        free(tt);
    }
    return 0;
}


static void test_condition_timedwait()
{
    condition_t cond;
    mutex_t lock;

    mutex_init(&lock);
    condition_init(&cond);

    mutex_lock(&lock);
    condition_timedwait(&cond, &lock, 10);
    mutex_unlock(&lock);

    mutex_destroy(&lock);
    condition_destroy(&cond);
}


int main(int argc, char **argv)
{
    test_waitgroup_single_thread();
    test_waitgroup_multi_threads();
    test_condition_timedwait();
}
