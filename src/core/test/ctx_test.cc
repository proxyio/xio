#include <gtest/gtest.h>
extern "C" {
#include "core/rio.h"
#include "core/accepter.h"
#include "core/proxy.h"
#include "sdk/c/io.h"
#include "runner/thread.h"
#include "sync/waitgroup.h"
}

#define PIOHOST "127.0.0.1:12300"
#define PROXYNAME "testapp"

extern int randstr(char *buff, int sz);
static int cnt = 0;

static int producer_worker(void *args) {
    char req[PAGE_SIZE], *resp;
    int64_t reqlen, resplen;
    int i;
    waitgroup_t *wg = (waitgroup_t *)args;
    producer_t *pt = producer_new(PIOHOST, PROXYNAME);

    for (i = 0; i < cnt; i++) {
	randstr(req, PAGE_SIZE);
	reqlen = rand() % PAGE_SIZE;
	EXPECT_EQ(0, producer_send_request(pt, req, reqlen));
	EXPECT_EQ(0, producer_recv_response(pt, &resp, &resplen));
	EXPECT_TRUE(reqlen == resplen);
	EXPECT_TRUE(memcmp(req, resp, reqlen) == 0);
	mem_free(resp, resplen);
    }

    producer_destroy(pt);
    waitgroup_done(wg);
    return 0;
}


static int comsumer_worker(void *args) {
    char *req, *rt;
    int i;
    int64_t sz, rt_sz;
    waitgroup_t *wg = (waitgroup_t *)args;
    comsumer_t *ct = comsumer_new(PIOHOST, PROXYNAME);

    for (i = 0; i < cnt; i++) {
	EXPECT_EQ(0, comsumer_recv_request(ct, &req, &sz, &rt, &rt_sz));
	EXPECT_EQ(0, comsumer_send_response(ct, req, sz, rt, rt_sz));
	mem_free(req, sz);
	mem_free(rt, rt_sz);
    }

    comsumer_destroy(ct);
    waitgroup_done(wg);    
    return 0;
}



static void acp_test() {
    thread_t t[2];
    acp_t acp = {};
    proxy_t py = {};
    waitgroup_t wg = {};

    proxy_init(&py);
    waitgroup_init(&wg);
    strcpy(py.proxyname, PROXYNAME);
    acp_init(&acp, 10);
    list_add(&py.acp_link, &acp.py_head);

    acp_start(&acp);
    EXPECT_EQ(0, acp_listen(&acp, PIOHOST));

    waitgroup_adds(&wg, 2);
    thread_start(&t[0], producer_worker, &wg);
    thread_start(&t[1], comsumer_worker, &wg);
    waitgroup_wait(&wg);
    thread_stop(&t[0]);
    thread_stop(&t[1]);

    EXPECT_EQ(0, acp_proxyto(&acp, PROXYNAME, PIOHOST));
    while (py.rsize != 2)
	usleep(1000);
    acp_stop(&acp);
    proxy_destroy(&py);
    acp_destroy(&acp);
    waitgroup_destroy(&wg);
}


TEST(ctx, accepter) {
    acp_test();
}
