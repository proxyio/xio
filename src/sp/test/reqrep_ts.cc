#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <string>
#include <xio/sp_reqrep.h>
#include <xio/socket.h>
extern "C" {
#include <sync/spin.h>
#include <runner/thread.h>
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
    host += "://127.0.0.1:18898";
    randstr(buf, sizeof(buf));
    BUG_ON((eid = sp_endpoint(SP_REQREP, SP_REQ)) < 0);
    for (i = 0; i < 1; i++) {
	BUG_ON((s = xconnect(host.c_str())) < 0);
	BUG_ON(sp_add(eid, s) < 0);
    }
    for (i = 0; i < 3; i++) {
	sbuf = rbuf = 0;
	sbuf = xallocmsg(sizeof(buf));
	memcpy(sbuf, buf, sizeof(buf));
	BUG_ON(sp_send(eid, sbuf) != 0);
	DEBUG_ON("producer %d send %d request: %10.10s", eid, i, sbuf);
	while (sp_recv(eid, &rbuf) != 0) {
	    usleep(10000);
	}
	DEBUG_ON("producer %d recv %d resp: %10.10s", eid, i, rbuf);
	DEBUG_ON("----------------------------------------");
	BUG_ON(xmsglen(rbuf) != sizeof(buf));
	BUG_ON(memcmp(rbuf, buf, sizeof(buf)) != 0);
	xfreemsg(rbuf);
    }
    sp_close(eid);
    return 0;
}


TEST(sp, reqrep) {
    string addr("://127.0.0.1:18898"), host;
    u32 i;
    thread_t t[3];
    const char *pf[] = {
	"tcp",
	"ipc",
	"inproc",
    };
    int s;
    int eid;
    char *ubuf;

    BUG_ON((eid = sp_endpoint(SP_REQREP, SP_REP)) < 0);
    for (i = 0; i < NELEM(t, thread_t); i++) {
	host = pf[i] + addr;
	BUG_ON((s = xlisten(host.c_str())) < 0);
	BUG_ON(sp_add(eid, s) < 0);
    }
    for (i = 0; i < NELEM(t, thread_t); i++) {
	thread_start(&t[i], req_thread, (void *)pf[i]);
    }

    for (i = 0; i < NELEM(t, thread_t) * 3; i++) {
	while (sp_recv(eid, &ubuf) != 0) {
	    usleep(10000);
	}
	DEBUG_ON("comsumer %d recv %d requst: %10.10s", eid, i, ubuf);
	BUG_ON(sp_send(eid, ubuf));
    }
    for (i = 0; i < NELEM(t, thread_t); i++) {
	thread_stop(&t[i]);
    }
    sp_close(eid);
}
