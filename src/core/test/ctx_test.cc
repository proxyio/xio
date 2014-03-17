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

extern int randstr(char *buff, int sz);
static int cnt = 10;

static volatile int comsumer_ok = false;

static int comsumer_worker(void *args) {
    char *req, *rt;
    uint32_t sz, rt_sz;
    pio_t *ct = pio_join(PIOHOST, PROXYNAME, COMSUMER);

    comsumer_ok = true;
    while (comsumer_ok) {
	if (comsumer_recv_request(ct, &req, &sz, &rt, &rt_sz) == 0) {
	    EXPECT_TRUE(comsumer_psend_response(ct, req, sz, rt, rt_sz) == 0);
	    mem_free(req, sz);
	    mem_free(rt, rt_sz);
	}
    }
    pio_close(ct);
    return 0;
}

static int test_producer1() {
    char req[PAGE_SIZE], *resp;
    uint32_t reqlen, resplen;
    int mycnt = cnt;
    pio_t *pt = pio_join(PIOHOST, PROXYNAME, PRODUCER);

    randstr(req, PAGE_SIZE);
    while (mycnt) {
	reqlen = rand() % PAGE_SIZE;
	EXPECT_TRUE(producer_psend_request(pt, req, reqlen, 0) == 0);
	EXPECT_TRUE(producer_precv_response(pt, &resp, &resplen) == 0);
	EXPECT_EQ(reqlen, resplen);
	EXPECT_TRUE(memcmp(req, resp, reqlen) == 0);
	mem_free(resp, resplen);
	mycnt--;
    }
    pio_close(pt);
    return 0;
}

static int test_producer2() {
    char req[PAGE_SIZE], *resp;
    uint32_t resplen;
    pio_t *pt = pio_join(PIOHOST, PROXYNAME, PRODUCER);

    randstr(req, PAGE_SIZE);
    for (int i = 0; i < cnt; i++)
	EXPECT_TRUE(producer_psend_request(pt, req, i, 0) == 0);
    for (int i = 0; i < cnt; i++) {
	EXPECT_TRUE(producer_precv_response(pt, &resp, &resplen) == 0);
	EXPECT_EQ(resplen, i);
	EXPECT_TRUE(memcmp(req, resp, resplen) == 0);
	mem_free(resp, resplen);
    }
    pio_close(pt);
    return 0;
}

static void acp_test() {
    thread_t t;
    acp_t acp = {};
    struct cf cf = {};

    cf.tp_workers = 1;
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
