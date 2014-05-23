#include <gtest/gtest.h>
#include <list>
extern "C" {
#include <utils/waitgroup.h>
#include <utils/taskpool.h>
}

using namespace std;

static int worker(void *args) {
    waitgroup_t *wg = (waitgroup_t *)args;
    usleep(10000);
    waitgroup_done(wg);
    return 0;
}

static void taskpool_test() {
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
}

TEST(runner, taskpool) {
    taskpool_test();
}
