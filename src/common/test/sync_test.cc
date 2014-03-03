#include <gtest/gtest.h>
#include <list>
extern "C" {
#include "runner/thread.h"
#include "sync/waitgroup.h"
}

using namespace std;

static int test_waitgroup_single_thread() {
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


static int task_func(void *data) {
    int i = 0;
    waitgroup_t *wg = (waitgroup_t *)data;
    for (i = 0; i < 100; i++)
	waitgroup_done(wg);
    return 0;
}

static int test_waitgroup_multi_threads() {
    waitgroup_t wg = {};
    int i, nthreads = 0;
    thread_t *tt = NULL;
    list<thread_t *> workers;
    list<thread_t *>::iterator it;
    
    nthreads = rand() % 20;
    for (i = 0; i < nthreads; i++) {
	EXPECT_TRUE((tt = thread_new()) != NULL);
	workers.push_back(tt);
    }
    waitgroup_adds(&wg, nthreads * 100);
    for (it = workers.begin(); it != workers.end(); ++it) {
	tt = *it;
	thread_start(tt, task_func, &wg);
    }
    waitgroup_wait(&wg);
    for (it = workers.begin(); it != workers.end(); ++it) {
	tt = *it;
	thread_stop(tt);
	free(tt);
    }
    return 0;
}


TEST(sync, waitgroup_single_thread) {
    test_waitgroup_single_thread();
}

TEST(sync, waitgroup_multi_threads) {
    test_waitgroup_multi_threads();
}
