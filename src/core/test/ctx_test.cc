#include <gtest/gtest.h>
extern "C" {
#include "core/role.h"
#include "core/accepter.h"
#include "core/grp.h"
}

#define PIOHOST "127.0.0.1:12300"

static void acp_test() {
    acp_t acp = {};
    grp_t grp = {};
    char grpname[GRPNAME_MAX] = "testapp";

    grp_init(&grp);
    strcpy(grp.grpname, grpname);
    acp_init(&acp, 10);
    list_add(&grp.acp_link, &acp.grp_head);

    acp_start(&acp);
    EXPECT_EQ(0, acp_listen(&acp, PIOHOST));
    EXPECT_EQ(0, acp_proxyto(&acp, grpname, PIOHOST));
    while (grp.rsize != 2)
	usleep(1000);
    acp_stop(&acp);
    grp_destroy(&grp);
    acp_destroy(&acp);
}


TEST(ctx, accepter) {
    acp_test();
}
