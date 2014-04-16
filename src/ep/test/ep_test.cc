#include <gtest/gtest.h>
extern "C" {
#include <ep/ep.h>
#include <ep/pxy.h>
#include <runner/thread.h>
#include <sync/waitgroup.h>
}

extern int randstr(char *buf, int len);

static const char *url;
static int cnt = 1;

static int test_producer(void *args) {
    int i;
    struct ep *producers[cnt];

    for (i = 0; i < cnt; i++) {
	BUG_ON(!(producers[i] = ep_new(DISPATCHER)));
	DEBUG_ON("producer %d connect start\n", i);
	BUG_ON((ep_connect(producers[i], url)) != 0);
	DEBUG_ON("producer %d connect ok\n", i);
    }
    for (i = 0; i < cnt; i++) {
	DEBUG_ON("producer %d connect closed\n", i);
	ep_close(producers[i]);
    }
    return 0;
}

static int test_comsumer(void *args) {
    int i;
    struct ep *comsumers[cnt];

    for (i = 0; i < cnt; i++) {
	BUG_ON(!(comsumers[i] = ep_new(RECEIVER)));
	DEBUG_ON("comsumer %d connect start\n", i);
	BUG_ON((ep_connect(comsumers[i], url)) != 0);
	DEBUG_ON("comsumer %d connect ok\n", i);
    }
    for (i = 0; i < cnt; i++) {
	DEBUG_ON("comsumer %d connect closed\n", i);
	ep_close(comsumers[i]);
    }
    return 0;
}

static int test_pxy(void *args) {
    struct pxy *y = (struct pxy *)args;
    return pxy_startloop(y);
}

static void test_proxy() {
    thread_t t[3];
    struct pxy y;

    pxy_init(&y);
    BUG_ON(pxy_listen(&y, url) != 0);
    thread_start(&t[2], test_pxy, &y);

    thread_start(&t[0], test_producer, NULL);
    thread_start(&t[1], test_comsumer, NULL);
    thread_stop(&t[0]);
    thread_stop(&t[1]);

    thread_stop(&t[2]);
    pxy_stoploop(&y);
    pxy_destroy(&y);
}


TEST(proxy, listen_connect) {
    DEBUG_ON("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    url = "default@net://127.0.0.1:18800";
    test_proxy();
    DEBUG_ON("yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy");
    url = "default@inp://xxxxxxxxxxxxxxx";
    test_proxy();
    DEBUG_ON("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");
    url = "default@ipc://group1.sock";
    test_proxy();
}



static const char *url1;

static int test_producer2(void *args) {
    waitgroup_t *wg = (waitgroup_t *)args;
    int i;
    char buf[16];
    char *payload, *payload2;
    struct ep *producer;

    BUG_ON(!(producer = ep_new(DISPATCHER)));
    BUG_ON((ep_connect(producer, url1)) != 0);
    waitgroup_done(wg);

    DEBUG_ON();
    for (i = 0; i < cnt; i++) {
	randstr(buf, sizeof(buf));
	BUG_ON(!(payload = channel_allocmsg(sizeof(buf))));
	memcpy(payload, buf, sizeof(buf));
	BUG_ON(ep_send_req(producer, payload));
	DEBUG_ON("producer send one req");

	BUG_ON(ep_recv_resp(producer, &payload2));
	DEBUG_ON("producer recv one resp");
	BUG_ON(sizeof(buf) != channel_msglen(payload2));
	BUG_ON(0 != memcmp(buf, payload2, sizeof(buf)));
	channel_freemsg(payload2);
    }
    ep_close(producer);
    return 0;
}

static int test_comsumer2(void *args) {
    waitgroup_t *wg = (waitgroup_t *)args;
    int i;
    char *payload, *rt;
    struct ep *comsumer = ep_new(RECEIVER);

    BUG_ON((ep_connect(comsumer, url1)) != 0);
    waitgroup_done(wg);

    for (i = 0; i < cnt; i++) {
	BUG_ON(ep_recv_req(comsumer, &payload, &rt));
	DEBUG_ON("comsumer recv one req");
	BUG_ON(ep_send_resp(comsumer, payload, rt));
	DEBUG_ON("comsumer send one resp");
    }
    ep_close(comsumer);
    return 0;
}

static int test_pxy2(void *args) {
    struct pxy *y = (struct pxy *)args;
    return pxy_startloop(y);
}

static void test_proxy2() {
    waitgroup_t wg;
    int i;
    thread_t comsumer[cnt];
    thread_t producer[cnt];
    thread_t pxy_thread;
    struct pxy y;

    pxy_init(&y);
    waitgroup_init(&wg);
    BUG_ON(pxy_listen(&y, url1) != 0);
    thread_start(&pxy_thread, test_pxy2, &y);
    waitgroup_adds(&wg, cnt);
    for (i = 0; i < cnt; i++) {
	thread_start(&comsumer[i], test_comsumer2, &wg);
    }
    waitgroup_wait(&wg);
    for (i = 0; i < cnt; i++)
	thread_start(&producer[i], test_producer2, &wg);

    for (i = 0; i < cnt; i++)
	thread_stop(&comsumer[i]);
    for (i = 0; i < cnt; i++)
	thread_stop(&producer[i]);
    thread_stop(&pxy_thread);
    pxy_stoploop(&y);
    pxy_destroy(&y);
    waitgroup_destroy(&wg);
}


TEST(proxy, send_recv) {
    DEBUG_ON("testing inproc proxy");
    url1 = "default@inp://xxxxxxxxxxxxxx2";
    test_proxy2();

    DEBUG_ON("testing net proxy xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    url1 = "default@net://127.0.0.1:18802";
    test_proxy2();

    DEBUG_ON("testing ipc proxy");
    url1 = "default@ipc://group2.sock";
    test_proxy2();
}
