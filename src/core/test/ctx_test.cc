#include <gtest/gtest.h>
extern "C" {
#include "core/role.h"
#include "core/accepter.h"
#include "core/grp.h"
}

#define PIOHOST "127.0.0.1:12300"

static void accepter_test() {
    accepter_t acp = {};
    grp_t grp = {};
    char grpname[GRPNAME_MAX] = "testapp";

    grp_init(&grp);
    strcpy(grp.grpname, grpname);
    accepter_init(&acp, 10);
    list_add(&grp.acp_link, &acp.grp_head);

    accepter_start(&acp);
    EXPECT_EQ(0, accepter_listen(&acp, PIOHOST));
    EXPECT_EQ(0, accepter_proxyto(&acp, grpname, PIOHOST));
    while (grp.rsize != 2) {
    }
    accepter_stop(&acp);
    grp_destroy(&grp);
    accepter_destroy(&acp);
}


TEST(ctx, accepter) {
    accepter_test();
}
