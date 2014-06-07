#include <utils/waitgroup.h>
#include <utils/taskpool.h>

static int worker(void *args)
{
    waitgroup_t *wg = (waitgroup_t *)args;
    usleep(10000);
    waitgroup_done(wg);
    return 0;
}

int main(int argc, char **argv)
{
    taskpool_t tp = {};
    waitgroup_t wg = {};

    waitgroup_init(&wg);
    taskpool_init(&tp, 10);
    taskpool_start(&tp);
    waitgroup_add(&wg);
    taskpool_run(&tp, worker, &wg);
    waitgroup_wait(&wg);
    taskpool_stop(&tp);
    taskpool_destroy(&tp);
    return 0;
}
