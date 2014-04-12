#include <gtest/gtest.h>
extern "C" {
#include <proxy/ep.h>
#include <proxy/pxy.h>
#include <runner/thread.h>
}

static const char **url;
static int pf = 0;
static int cnt = 10;

static int test_producer(void *args) {
    int i;
    struct ep *producers[cnt];

    for (i = 0; i < cnt; i++) {
	BUG_ON(!(producers[i] = ep_new(PRODUCER)));
	BUG_ON((ep_connect(producers[i], url[1])) != 0);
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
	BUG_ON((ep_connect(comsumers[i], url[1])) != 0);
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

    BUG_ON(pxy_listen(y, pf, url[0]) != 0);
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
    const char *url1[2] = {
	"127.0.0.1:18800",
	"group1@net://127.0.0.1:18800",
    };
    pf = PF_NET;
    url = url1;
    test_proxy();
}

TEST(proxy, inproc) {
    const char *url2[2] = {
	"xxxxxxxxxxxxxxx",
	"group1@inp://xxxxxxxxxxxxxxx",
    };
    pf = PF_INPROC;
    url = url2;
    test_proxy();
}

TEST(proxy, ipc) {
    const char *url3[2] = {
	"group1.sock",
	"group1@ipc://group1.sock",
    };
    pf = PF_IPC;
    url = url3;
    test_proxy();
}
