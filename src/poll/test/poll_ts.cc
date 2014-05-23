#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <string>
extern "C" {
#include <utils/spin.h>
#include <utils/thread.h>
#include <xio/poll.h>
#include <xio/socket.h>
}

using namespace std;

extern int randstr(char *buf, int len);
static int cnt = 10;

static void xclient(string pf) {
    int sfd, i;
    int64_t nbytes;
    char buf[1024] = {};
    char *ubuf;
    string host(pf + "://127.0.0.1:18897");

    randstr(buf, 1024);
    BUG_ON((sfd = xconnect(host.c_str())) < 0);
    for (i = 0; i < cnt; i++) {
	nbytes = rand() % 1024;
	ubuf = xallocubuf(nbytes);
	memcpy(ubuf, buf, nbytes);
	BUG_ON(0 != xsend(sfd, ubuf));
	BUG_ON(0 != xrecv(sfd, &ubuf));
	DEBUG_OFF("%d recv response", sfd);
	assert(memcmp(ubuf, buf, nbytes) == 0);
	xfreeubuf(ubuf);
    }
    xclose(sfd);
}

static int xclient_thread(void *arg) {
    xclient("tcp");
    xclient("ipc");
    xclient("inproc");
    return 0;
}

TEST(xpoll, select) {
    int i, j;
    char *ubuf = 0;
    int afd, sfd, tmp;
    thread_t cli_thread = {};

    BUG_ON((afd = xlisten("tcp+ipc+inproc://127.0.0.1:18897")) < 0);
    thread_start(&cli_thread, xclient_thread, 0);
    usleep(100000);
    BUG_ON(xselect(XPOLLIN, 1, &afd, 1, &tmp) <= 0);
    for (j = 0; j < 3; j++) {
        BUG_ON((sfd = xaccept(afd)) < 0);
        for (i = 0; i < cnt; i++) {
	    while (xselect(XPOLLIN, 1, &sfd, 1, &tmp) <= 0)
	        usleep(10000);
	    BUG_ON(tmp != sfd);
	    BUG_ON(0 != xrecv(sfd, &ubuf));
	    BUG_ON(0 != xsend(sfd, ubuf));
        }
        xclose(sfd);
    }
    thread_stop(&cli_thread);
    xclose(afd);
}
