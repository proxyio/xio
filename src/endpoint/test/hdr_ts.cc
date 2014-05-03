#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <string>
extern "C" {
#include <sync/spin.h>
#include <runner/thread.h>
#include <xio/socket.h>
#include <xio/endpoint.h>
#include <endpoint/ep_hdr.h>
}

using namespace std;

extern int randstr(char *buf, int len);

struct ts_hdr {
    struct ephdr h;
    struct epr rt[4];
    struct uhdr uh;
    char ubuf[128];
};

TEST(endpoint, eid) {
    int eid1, eid2;

    eid1 = xep_open(XCONNECTOR);
    eid2 = xep_open(XCONNECTOR);
    BUG_ON(eid1 == eid2);
}

TEST(endpoint, hdr) {
    struct ts_hdr th;
    char *ubuf;

    th.h.ttl = 4;
    th.h.go = 1;
    th.h.size = sizeof(th.ubuf);
    th.uh.ephdr_off = sizeof(th.h) + sizeof(th.rt) + sizeof(th.uh);

    BUG_ON(ephdr_rtlen(&th.h) != sizeof(th.rt));
    BUG_ON(ephdr_ctlen(&th.h) != sizeof(th.h) + sizeof(th.rt) + sizeof(th.uh));
    BUG_ON(ephdr_dlen(&th.h) != sizeof(th.ubuf));
    BUG_ON(ubuf2ephdr(th.ubuf) != &th.h);
    BUG_ON(ephdr2ubuf(&th.h) != th.ubuf);
    BUG_ON(ephdr_uhdr(&th.h) != &th.uh);
    ubuf = xep_allocubuf(0, 128);
    randstr(ubuf, 128);
    xep_freeubuf(ubuf);
}

static int producer_thread(void *args) {
    string host;
    char buf[128];
    int s;
    int i;
    int eid;
    char *sbuf, *rbuf;

    host.assign((char *)args);
    host += "://127.0.0.1:18898";
    randstr(buf, sizeof(buf));
    BUG_ON((eid = xep_open(XEP_PRODUCER)) < 0);
    for (i = 0; i < 3; i++) {
	BUG_ON((s = xconnect(host.c_str())) < 0);
	BUG_ON(xep_add(eid, s) < 0);
    }
    for (i = 0; i < 30; i++) {
	sbuf = rbuf = 0;
	sbuf = xep_allocubuf(0, sizeof(buf));
	memcpy(sbuf, buf, sizeof(buf));
	BUG_ON(xep_send(eid, sbuf) != 0);
	DEBUG_OFF("producer send %d request: %10.10s", i, sbuf);
	while (xep_recv(eid, &rbuf) != 0) {
	    usleep(10000);
	}
	DEBUG_OFF("producer recv %d resp: %10.10s", i, rbuf);
	DEBUG_OFF("----------------------------------------");
	BUG_ON(xep_ubuflen(rbuf) != sizeof(buf));
	BUG_ON(memcmp(rbuf, buf, sizeof(buf)) != 0);
	xep_freeubuf(rbuf);
    }
    xep_close(eid);
    return 0;
}

volatile static int proxy_stopped = 1;

static int proxy_thread(void *args) {
    string fronthost("tcp+inproc+ipc://127.0.0.1:18898");
    string backhost("tcp+inproc+ipc://127.0.0.1:18899");
    int s;
    int front_eid, back_eid;
    
    BUG_ON((front_eid = xep_open(XEP_COMSUMER)) < 0);
    BUG_ON((back_eid = xep_open(XEP_PRODUCER)) < 0);

    BUG_ON((s = xlisten(fronthost.c_str())) < 0);
    BUG_ON(xep_add(front_eid, s) < 0);

    BUG_ON((s = xlisten(backhost.c_str())) < 0);
    BUG_ON(xep_add(back_eid, s) < 0);

    BUG_ON(xep_setopt(front_eid, XEP_DISPATCHTO, &back_eid, sizeof(back_eid)));
    proxy_stopped = 0;
    while (!proxy_stopped)
	usleep(20000);
    BUG_ON(xep_setopt(front_eid, XEP_DISPATCHTO, &back_eid, sizeof(back_eid)));
    xep_close(back_eid);
    xep_close(front_eid);
    return 0;
}

TEST(endpoint, proxy) {
    string addr("://127.0.0.1:18899"), host;
    u32 i;
    thread_t pyt;
    thread_t t[3];
    const char *pf[] = {
	"tcp",
	"ipc",
	"inproc",
    };
    int s;
    int eid;
    char *ubuf, *ubuf2;

    thread_start(&pyt, proxy_thread, 0);
    while (proxy_stopped)
	usleep(20000);
    BUG_ON((eid = xep_open(XEP_COMSUMER)) < 0);
    for (i = 0; i < NELEM(t, thread_t); i++) {
	host = pf[i] + addr;
	BUG_ON((s = xconnect(host.c_str())) < 0);
	BUG_ON(xep_add(eid, s) < 0);
    }
    for (i = 0; i < NELEM(t, thread_t); i++) {
	thread_start(&t[i], producer_thread, (void *)pf[i]);
    }

    for (i = 0; i < NELEM(t, thread_t) * 30; i++) {
	while (xep_recv(eid, &ubuf) != 0) {
	    usleep(1000);
	}
	DEBUG_OFF("comsumer recv %d requst: %10.10s", i, ubuf);
	ubuf2 = xep_allocubuf(XEPUBUF_CLONEHDR, xep_ubuflen(ubuf), ubuf);
	memcpy(ubuf2, ubuf, xep_ubuflen(ubuf));
	xep_freeubuf(ubuf);
	BUG_ON(xep_send(eid, ubuf2));
    }

    for (i = 0; i < NELEM(t, thread_t); i++) {
	thread_stop(&t[i]);
    }
    xep_close(eid);
    proxy_stopped = 1;
    thread_stop(&pyt);
}
