#include <gtest/gtest.h>
extern "C" {
#include "core/role.h"
#include "core/accepter.h"
#include "core/grp.h"
}

#define PIOHOST "127.0.0.1:12300"

static void accepter_test() {
    accepter_t acp = {};
    taskpool_t tp = {};
    grp_t grp = {};
    char grpname[GRPNAME_MAX] = "testapp";

    grp_init(&grp);
    strcpy(grp.grpname, grpname);
    taskpool_init(&tp, 10);
    accepter_init(&acp);
    list_add(&grp.ctx_link, &acp.grp_head);

    taskpool_start(&tp);
    accepter_start(&acp, &tp);
    EXPECT_EQ(0, accepter_listen(&acp, PIOHOST));
    EXPECT_EQ(0, accepter_proxyto(&acp, grpname, PIOHOST));
    while (grp.rsize != 2) {
    }
    
    accepter_stop(&acp);
    taskpool_stop(&tp);
    grp_destroy(&grp);
    accepter_destroy(&acp);
    taskpool_destroy(&tp);
}


TEST(ctx, accepter) {
    accepter_test();
}
