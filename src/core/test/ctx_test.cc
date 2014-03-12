#include <gtest/gtest.h>
extern "C" {
#include "core/role.h"
#include "core/accepter.h"
#include "core/grp.h"
#include "sdk/c/io.h"
#include "runner/thread.h"
#include "sync/waitgroup.h"
}

#define PIOHOST "127.0.0.1:12300"
#define GRPNAME "testapp"

static int producer_worker(void *args) {
    waitgroup_t *wg = (waitgroup_t *)args;
    producer_t *pt = producer_new(PIOHOST, GRPNAME);
    producer_destroy(pt);
    waitgroup_done(wg);
    return 0;
}


static int comsumer_worker(void *args) {
    waitgroup_t *wg = (waitgroup_t *)args;
    comsumer_t *ct = comsumer_new(PIOHOST, GRPNAME);
    comsumer_destroy(ct);
    waitgroup_done(wg);    
    return 0;
}



static void acp_test() {
    thread_t t[2];
    acp_t acp = {};
    grp_t grp = {};
    waitgroup_t wg = {};

    grp_init(&grp);
    waitgroup_init(&wg);
    strcpy(grp.grpname, GRPNAME);
    acp_init(&acp, 10);
    list_add(&grp.acp_link, &acp.grp_head);

    acp_start(&acp);
    EXPECT_EQ(0, acp_listen(&acp, PIOHOST));

    waitgroup_adds(&wg, 2);
    thread_start(&t[0], producer_worker, &wg);
    thread_start(&t[1], comsumer_worker, &wg);
    waitgroup_wait(&wg);
    thread_stop(&t[0]);
    thread_stop(&t[1]);

    
    EXPECT_EQ(0, acp_proxyto(&acp, GRPNAME, PIOHOST));
    while (grp.rsize != 2)
	usleep(1000);
    acp_stop(&acp);
    grp_destroy(&grp);
    acp_destroy(&acp);
    waitgroup_destroy(&wg);
}


TEST(ctx, accepter) {
    acp_test();
}
