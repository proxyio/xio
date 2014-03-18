#include <gtest/gtest.h>
extern "C" {
#include "core/role.h"
#include "core/accepter.h"
#include "core/proxy.h"
#include "sdk/c/io.h"
#include "runner/thread.h"
#include "sync/waitgroup.h"
}

#define PIOHOST "127.0.0.1:12300"
#define PROXYNAME "testapp"

static char page[4096];
extern int randstr(char *buff, int sz);
static int cnt = 1;

static volatile int comsumer_ok = false;

static int comsumer_worker(void *args) {
    pmsg_t *msg;
    pio_t *io = pio_join_comsumer(PIOHOST, PROXYNAME);

    comsumer_ok = true;
    usleep(10000);
    while (comsumer_ok) {
	if (pio_recvmsg(io, &msg) == 0) {
	    EXPECT_TRUE(pio_sendmsg(io, msg) == 0);
	    free_pio_msg_and_vec(msg);
	}
    }
    pio_close(io);
    return 0;
}

static int test_producer1() {
    int mycnt = cnt;
    pmsg_t *resp;
    pmsg_t *req = alloc_pio_msg(1);
    pio_t *io = pio_join_producer(PIOHOST, PROXYNAME);

    while (mycnt) {
	req->vec[0].iov_base = page;
	req->vec[0].iov_len = rand() % PAGE_SIZE;
	EXPECT_TRUE(pio_sendmsg(io, req) == 0);
	while (pio_recvmsg(io, &resp) != 0)
	    usleep(10000);
	EXPECT_EQ(req->vec[0].iov_len, resp->vec[0].iov_len);
	EXPECT_TRUE(memcmp(page, resp->vec[0].iov_base, resp->vec[0].iov_len) == 0);
	free_pio_msg_and_vec(resp);
	mycnt--;
    }
    free_pio_msg(req);
    pio_close(io);
    return 0;
}

static int test_producer2() {
    pmsg_t *resp;
    pmsg_t *req = alloc_pio_msg(1);
    pio_t *io = pio_join_producer(PIOHOST, PROXYNAME);

    for (int i = 0; i < cnt; i++) {
	req->vec[0].iov_base = page;
	req->vec[0].iov_len = i;
	EXPECT_TRUE(pio_sendmsg(io, req) == 0);
    }
    for (int i = 0; i < cnt; i++) {
	while (pio_recvmsg(io, &resp) != 0)
	    usleep(10000);
	EXPECT_EQ(resp->vec[0].iov_len, i);
	EXPECT_TRUE(memcmp(resp->vec[0].iov_base, page, i) == 0);
	free_pio_msg_and_vec(resp);
    }
    free_pio_msg(req);
    pio_close(io);
    return 0;
}

static void acp_test() {
    thread_t t;
    acp_t acp = {};
    struct cf cf = {};

    randstr(page, sizeof(page));
    cf.max_cpus = 1;
    cf.el_io_size = 100;
    cf.el_wait_timeout = 1;
    acp_init(&acp, &cf);
    EXPECT_EQ(0, acp_listen(&acp, PIOHOST));
    acp_start(&acp);

    thread_start(&t, comsumer_worker, NULL);
    while (!comsumer_ok) {
    }
    test_producer1();
    test_producer2();

    comsumer_ok = false;
    thread_stop(&t);
    acp_stop(&acp);
    acp_destroy(&acp);
}


TEST(ctx, accepter) {
    acp_test();
}
