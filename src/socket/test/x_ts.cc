#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <string>
extern "C" {
#include <sync/spin.h>
#include <runner/thread.h>
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
    char *payload;
    string host(pf + "://127.0.0.1:18894");

    randstr(buf, 1024);
    BUG_ON((sfd = xconnect(host.c_str())) < 0);
    for (i = 0; i < cnt; i++) {
	nbytes = rand() % 1024;
	payload = xallocmsg(nbytes);
	memcpy(payload, buf, nbytes);
	BUG_ON(0 != xsend(sfd, payload));
	BUG_ON(0 != xrecv(sfd, &payload));
	DEBUG_OFF("%d recv response", sfd);
	assert(memcmp(payload, buf, nbytes) == 0);
	xfreemsg(payload);
    }
    xclose(sfd);
}

static int xclient_thread(void *arg) {
    xclient("tcp");
    xclient("inproc");
    return 0;
}

static void xserver() {
    int i, j;
    int afd, sfd;
    thread_t cli_thread = {};
    char *payload;
    string host("tcp+inproc://127.0.0.1:18894");
    
    BUG_ON((afd = xlisten(host.c_str())) < 0);
    thread_start(&cli_thread, xclient_thread, 0);

    for (j = 0; j < 2; j++) {
	BUG_ON((sfd = xaccept(afd)) < 0);
	DEBUG_OFF("xserver accept %d", sfd);
	for (i = 0; i < cnt; i++) {
	    BUG_ON(0 != xrecv(sfd, &payload));
	    DEBUG_OFF("%d recv", sfd);
	    BUG_ON(0 != xsend(sfd, payload));
	}
    }
    thread_stop(&cli_thread);
    DEBUG_OFF("%s", "xclient thread return");
    xclose(afd);
}

static struct xpoll_t *po = 0;

static void xclient2(string pf) {
    int i;
    int sfd[cnt];
    struct xpoll_event event[cnt];
    string host(pf + "://127.0.0.1:18895");

    for (i = 0; i < cnt; i++) {
	BUG_ON((sfd[i] = xconnect(host.c_str())) < 0);
	event[i].xd = sfd[i];
	event[i].self = po;
	event[i].care = XPOLLIN|XPOLLOUT|XPOLLERR;
	assert(xpoll_ctl(po, XPOLL_ADD, &event[i]) == 0);
    }
    for (i = 0; i < cnt; i++)
	xclose(sfd[i]);
}

static int xclient_thread2(void *arg) {
    xclient2("tcp");
    xclient2("ipc");
    xclient2("inproc");
    return 0;
}

static void xserver2() {
    int i, j, mycnt;
    int afd, sfd[cnt];
    thread_t cli_thread = {};
    struct xpoll_event event[cnt];

    po = xpoll_create();
    DEBUG_OFF("%p", po);
    BUG_ON((afd = xlisten("tcp+ipc+inproc://127.0.0.1:18895")) < 0);
    thread_start(&cli_thread, xclient_thread2, 0);
    event[0].xd = afd;
    event[0].self = po;
    event[0].care = XPOLLERR;
    BUG_ON(xpoll_ctl(po, XPOLL_ADD, &event[0]) != 0);

    for (j = 0; j < 3; j++) {
	for (i = 0; i < cnt; i++) {
	    BUG_ON((sfd[i] = xaccept(afd)) < 0);
	    DEBUG_OFF("%d", sfd[i]);
	    event[i].xd = sfd[i];
	    event[i].self = po;
	    event[i].care = XPOLLIN|XPOLLOUT|XPOLLERR;
	    BUG_ON(xpoll_ctl(po, XPOLL_ADD, &event[i]) != 0);
	}
	mycnt = rand() % (cnt);
	for (i = 0; i < mycnt; i++) {
	    DEBUG_OFF("%d", sfd[i]);
	    event[i].xd = sfd[i];
	    event[i].self = po;
	    event[i].care = XPOLLIN|XPOLLOUT|XPOLLERR;
	    BUG_ON(xpoll_ctl(po, XPOLL_DEL, &event[i]) != 0);
	    BUG_ON(xpoll_ctl(po, XPOLL_DEL, &event[i]) != -1);
	}
	for (i = 0; i < cnt; i++)
	    xclose(sfd[i]);
    }

    xpoll_close(po);
    thread_stop(&cli_thread);
    xclose(afd);
}

static void xsock_test(int count) {
    while (count-- > 0) {
	xserver();
	xserver2();
    }
}


TEST(xsock, vf) {
    xsock_test(1);
}





static int cnt2 = 100;

static void inproc_client2() {
    int sfd, i;

    for (i = 0; i < cnt2/2; i++) {
	if ((sfd = xconnect("inproc://b_inproc")) < 0)
	    break;
	xclose(sfd);
    }
}

static int inproc_client_thread2(void *args) {
    inproc_client2();
    return 0;
}

static void inproc_client3() {
    int sfd, i;

    for (i = 0; i < cnt2/2; i++) {
	if ((sfd = xconnect("inproc://b_inproc")) < 0)
	    break;
	xclose(sfd);
    }
}

static int inproc_client_thread3(void *args) {
    inproc_client3();
    return 0;
}

static void inproc_server_thread2() {
    int i, afd, sfd;
    thread_t cli_thread[2] = {};

    BUG_ON((afd = xlisten("inproc://b_inproc")) < 0);
    thread_start(&cli_thread[0], inproc_client_thread2, NULL);
    thread_start(&cli_thread[1], inproc_client_thread3, NULL);

    for (i = 0; i < cnt2 - 10; i++) {
	BUG_ON((sfd = xaccept(afd)) < 0);
	xclose(sfd);
    }
    xclose(afd);
    thread_stop(&cli_thread[0]);
    thread_stop(&cli_thread[1]);
}

TEST(xsock, inproc) {
    inproc_server_thread2();
}

