#include <gtest/gtest.h>
extern "C" {
#include "os/epoll.h"
}

static int epoll_test() {
    epoll_t el = {};

    epoll_init(&el, 1024, 100);
    epoll_destroy(&el);
    return 0;
}


TEST(os, epoll) {
    epoll_test();
}
