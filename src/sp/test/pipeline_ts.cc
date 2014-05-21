#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <string>
extern "C" {
#include <sync/spin.h>
#include <runner/thread.h>
#include <xio/socket.h>
#include <xio/cmsghdr.h>
#include <xio/sp_reqrep.h>
}

using namespace std;

extern int randstr(char *buf, int len);

static int req_thread(void *args) {
    string host;
    char buf[128];
    int s;
    int i;
    int eid;
    char *sbuf, *rbuf;

    host.assign((char *)args);
    host += "://127.0.0.1:18900";
    randstr(buf, sizeof(buf));
    BUG_ON((eid = sp_endpoint(SP_REQREP, SP_REQ)) < 0);
    for (i = 0; i < 3; i++) {
	BUG_ON((s = xconnect(host.c_str())) < 0);
	BUG_ON(sp_add(eid, s) < 0);
    }
    for (i = 0; i < 3; i++) {
	sbuf = rbuf = 0;
	sbuf = xallocubuf(sizeof(buf));
	memcpy(sbuf, buf, sizeof(buf));
	BUG_ON(sp_send(eid, sbuf) != 0);
	DEBUG_ON("producer send %d request: %10.10s", i, sbuf);
	while (sp_recv(eid, &rbuf) != 0) {
	    usleep(10000);
	}
	DEBUG_ON("producer recv %d resp: %10.10s", i, rbuf);
	DEBUG_ON("----------------------------------------");
	BUG_ON(xubuflen(rbuf) != sizeof(buf));
	BUG_ON(memcmp(rbuf, buf, sizeof(buf)) != 0);
	xfreeubuf(rbuf);
    }
    sp_close(eid);
    return 0;
}

volatile static int proxy_stopped = 1;

static int proxy_thread(void *args) {
    string fronthost("tcp+inproc+ipc://127.0.0.1:18900");
    string backhost("tcp+inproc+ipc://127.0.0.1:18899");
    int s;
    int front_eid, back_eid;
    
    BUG_ON((front_eid = sp_endpoint(SP_REQREP, SP_REP)) < 0);
    BUG_ON((back_eid = sp_endpoint(SP_REQREP, SP_REQ)) < 0);

    BUG_ON((s = xlisten(fronthost.c_str())) < 0);
    BUG_ON(sp_add(front_eid, s) < 0);

    BUG_ON((s = xlisten(backhost.c_str())) < 0);
    BUG_ON(sp_add(back_eid, s) < 0);

    BUG_ON(sp_setopt(back_eid, SP_PROXY, &front_eid, sizeof(front_eid)));
    proxy_stopped = 0;
    while (!proxy_stopped)
	usleep(20000);
    sp_close(back_eid);
    sp_close(front_eid);
    return 0;
}

TEST(sp, pipeline) {
    string addr("://127.0.0.1:18899"), host;
    u32 i;
    thread_t pyt;
    thread_t t[1];
    const char *pf[] = {
	"tcp",
	"ipc",
	"inproc",
    };
    int s;
    int eid;
    int cmsgnum;
    char *ubuf;

    thread_start(&pyt, proxy_thread, 0);
    while (proxy_stopped)
	usleep(20000);
    BUG_ON((eid = sp_endpoint(SP_REQREP, SP_REP)) < 0);
    for (i = 0; i < NELEM(t, thread_t); i++) {
	host = pf[i] + addr;
	BUG_ON((s = xconnect(host.c_str())) < 0);
	BUG_ON(sp_add(eid, s) < 0);
    }
    for (i = 0; i < NELEM(t, thread_t); i++) {
	thread_start(&t[i], req_thread, (void *)pf[i]);
    }

    for (i = 0; i < NELEM(t, thread_t) * 3; i++) {
	while (sp_recv(eid, &ubuf) != 0) {
	    usleep(1000);
	}
	DEBUG_ON("comsumer recv %d requst: %10.10s", i, ubuf);
	BUG_ON(xmsgctl(ubuf, XMSG_CMSGNUM, &cmsgnum));
	BUG_ON(cmsgnum != 3);
	BUG_ON(sp_send(eid, ubuf));
    }

    for (i = 0; i < NELEM(t, thread_t); i++) {
	thread_stop(&t[i]);
    }
    sp_close(eid);
    proxy_stopped = 1;
    thread_stop(&pyt);
}
