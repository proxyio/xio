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

    taskpool_init(&tp, 10);
    taskpool_start(&tp);
    accepter_init(&acp);
    accepter_start(&acp, &tp);
    accepter_listen(&acp, PIOHOST);
    accepter_stop(&acp);
    taskpool_stop(&tp);
    accepter_destroy(&acp);
    taskpool_destroy(&tp);
}


TEST(ctx, accepter) {
    accepter_test();
}
