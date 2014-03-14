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
static int cnt = 10;

static int producer_worker(void *args) {
    char req[PAGE_SIZE], *resp;
    int64_t reqlen, resplen;
    int mycnt = cnt;
    waitgroup_t *wg = (waitgroup_t *)args;
    producer_t *pt = producer_new(PIOHOST, PROXYNAME);

    randstr(req, PAGE_SIZE);
    reqlen = rand() % PAGE_SIZE;
    EXPECT_EQ(0, producer_send_request(pt, req, reqlen));
    mycnt--;
    while (mycnt) {
	if (producer_recv_response(pt, &resp, &resplen) == 0) {
	    EXPECT_EQ(reqlen, resplen);
	    EXPECT_TRUE(memcmp(req, resp, reqlen) == 0);
	    mem_free(resp, resplen);
	    randstr(req, PAGE_SIZE);
	    reqlen = rand() % PAGE_SIZE;
	    EXPECT_EQ(0, producer_send_request(pt, req, reqlen));
	    mycnt--;
	}
    }
    producer_destroy(pt);
    waitgroup_done(wg);
    return 0;
}

static volatile int comsumer_ok = 0;

static int comsumer_worker(void *args) {
    char *req, *rt;
    int mycnt = cnt;
    int64_t sz, rt_sz;
    waitgroup_t *wg = (waitgroup_t *)args;
    comsumer_t *ct = comsumer_new(PIOHOST, PROXYNAME);

    comsumer_ok = 1;
    while (mycnt) {
	if (comsumer_recv_request(ct, &req, &sz, &rt, &rt_sz) == 0) {
	    comsumer_send_response(ct, req, sz, rt, rt_sz);
	    mem_free(req, sz);
	    mem_free(rt, rt_sz);
	    mycnt--;
	}
    }
    comsumer_destroy(ct);
    waitgroup_done(wg);    
    return 0;
}



static void acp_test() {
    thread_t t;
    acp_t acp = {};
    proxy_t py = {};
    waitgroup_t wg = {};

    proxy_init(&py);
    waitgroup_init(&wg);
    strcpy(py.proxyname, PROXYNAME);
    acp_init(&acp, 1);
    list_add(&py.acp_link, &acp.py_head);

    acp_start(&acp);
    EXPECT_EQ(0, acp_listen(&acp, PIOHOST));

    waitgroup_adds(&wg, 2);
    thread_start(&t, comsumer_worker, &wg);
    while (!comsumer_ok) {
    }
    producer_worker(&wg);
    waitgroup_wait(&wg);
    thread_stop(&t);

    EXPECT_EQ(0, acp_proxyto(&acp, PROXYNAME, PIOHOST));
    while (py.rsize != 2)
	usleep(1000);
    acp_stop(&acp);
    proxy_destroy(&py);
    acp_destroy(&acp);
    waitgroup_destroy(&wg);
}


TEST(ctx, accepter) {
    uuid_t uuid;
    EXPECT_TRUE(sizeof(uuid) == sizeof(uuid_t));
    acp_test();
}
