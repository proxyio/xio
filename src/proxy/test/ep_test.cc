#include <gtest/gtest.h>
extern "C" {
#include <proxy/ep.h>
#include <proxy/pxy.h>
#include <runner/thread.h>
}

static const char *url;
static int cnt = 10;

static int test_producer(void *args) {
    int i;
    struct ep *producers[cnt];

    for (i = 0; i < cnt; i++) {
	BUG_ON(!(producers[i] = ep_new(PRODUCER)));
	BUG_ON((ep_connect(producers[i], url)) != 0);
	//printf("producer %d connect ok\n", i);
    }
    for (i = 0; i < cnt; i++) {
	//printf("producer %d connect closed\n", i);
	ep_close(producers[i]);
    }
    return 0;
}

static int test_comsumer(void *args) {
    int i;
    struct ep *comsumers[cnt];

    for (i = 0; i < cnt; i++) {
	BUG_ON(!(comsumers[i] = ep_new(COMSUMER)));
	BUG_ON((ep_connect(comsumers[i], url)) != 0);
	//printf("comsumer %d connect ok\n", i);
    }
    for (i = 0; i < cnt; i++) {
	//printf("comsumer %d connect closed\n", i);
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
    struct pxy *y = pxy_new();

    BUG_ON(pxy_listen(y, url) != 0);
    thread_start(&t[0], test_producer, NULL);
    thread_start(&t[1], test_comsumer, NULL);
    thread_start(&t[2], test_pxy, y);
    thread_stop(&t[0]);
    thread_stop(&t[1]);
    thread_stop(&t[2]);
    pxy_stoploop(y);
    pxy_free(y);
}


TEST(proxy, net) {
    url = "group1@net://127.0.0.1:18800";
    test_proxy();
}

TEST(proxy, inproc) {
    url = "group1@inp://xxxxxxxxxxxxxxx";
    test_proxy();
}

TEST(proxy, ipc) {
    url = "group1@ipc://group1.sock";
    test_proxy();
}


static int test_producer2(void *args) {
    int i;
    struct ep *producer;

    BUG_ON(!(producer = ep_new(PRODUCER)));
    BUG_ON((ep_connect(producer, url)) != 0);
    for (i = 0; i < cnt; i++) {
    }
    for (i = 0; i < cnt; i++) {
    }
    ep_close(producer);
    return 0;
}

static int test_comsumer2(void *args) {
    int i;
    struct ep *comsumer = ep_new(COMSUMER);

    BUG_ON((ep_connect(comsumer, url)) != 0);
    for (i = 0; i < cnt; i++) {
    }
    for (i = 0; i < cnt; i++) {
    }
    ep_close(comsumer);
    return 0;
}

static int test_pxy2(void *args) {
    struct pxy *y = (struct pxy *)args;
    return pxy_startloop(y);
}

static void test_proxy2() {
    int i;
    thread_t comsumer[cnt];
    thread_t producer[cnt];
    thread_t pxy_thread;
    struct pxy *y = pxy_new();

    BUG_ON(pxy_listen(y, url) != 0);
    thread_start(&pxy_thread, test_pxy2, y);
    for (i = 0; i < cnt; i++)
	thread_start(&comsumer[i], test_comsumer2, NULL);
    for (i = 0; i < cnt; i++)
	thread_start(&producer[i], test_producer2, NULL);
    for (i = 0; i < cnt; i++)
	thread_stop(&comsumer[i]);
    for (i = 0; i < cnt; i++)
	thread_stop(&producer[i]);
    thread_stop(&pxy_thread);
    pxy_stoploop(y);
    pxy_free(y);
}


TEST(proxy, net2) {
    url = "group1@net://127.0.0.1:18802";
    test_proxy2();
}

TEST(proxy, inproc2) {
    url = "group1@inp://xxxxxxxxxxxxxx2";
    test_proxy2();
}

TEST(proxy, ipc2) {
    url = "group1@ipc://group2.sock";
    test_proxy2();
}
