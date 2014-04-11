#include <gtest/gtest.h>
extern "C" {
#include <proxy/ep.h>
#include <proxy/pxy.h>
#include <runner/thread.h>
}

#define LISTEN_HOST1 "127.0.0.1:18800"
#define LISTEN_HOST2 "127.0.0.1:18801"
static int pf = 0;
static int cnt = 10;

static int test_producer(void *args) {
    int i;
    struct ep *producers[cnt];

    for (i = 0; i < cnt; i++) {
	BUG_ON(!(producers[i] = ep_new(PRODUCER)));
	BUG_ON((ep_connect(producers[i], "group1@net:://"LISTEN_HOST1)) != 0);
	printf("producer %d connect ok\n", i);
    }
    for (i = 0; i < cnt; i++) {
	printf("producer %d connect closed\n", i);
	ep_close(producers[i]);
    }
    return 0;
}

static int test_comsumer(void *args) {
    int i;
    struct ep *comsumers[cnt];

    for (i = 0; i < cnt; i++) {
	BUG_ON(!(comsumers[i] = ep_new(COMSUMER)));
	BUG_ON((ep_connect(comsumers[i], "group1@net:://"LISTEN_HOST1)) != 0);
	printf("comsumer %d connect ok\n", i);
    }
    for (i = 0; i < cnt; i++) {
	printf("comsumer %d connect closed\n", i);
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

    BUG_ON(pxy_listen(y, pf, LISTEN_HOST1) != 0);
    thread_start(&t[0], test_producer, NULL);
    thread_start(&t[1], test_comsumer, NULL);
    thread_start(&t[2], test_pxy, y);
    thread_stop(&t[0]);
    thread_stop(&t[1]);
    thread_stop(&t[2]);
    pxy_stoploop(y);
    pxy_free(y);
}


TEST(proxy, pxy) {
    pf = PF_NET;
    test_proxy();
}
