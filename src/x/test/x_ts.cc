#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
extern "C" {
#include <sync/spin.h>
#include <runner/thread.h>
#include <x/xsock.h>
}

extern int randstr(char *buf, int len);

static int cnt = 10;

static void xclient(int pf) {
    int sfd, i;
    int buf_sz = 0;
    int64_t nbytes;
    char buf[1024] = {};
    char *payload;

    randstr(buf, 1024);
    ASSERT_TRUE((sfd = xconnect(pf, "127.0.0.1:18894")) >= 0);
    xsetopt(sfd, XSNDBUF, &buf_sz, sizeof(buf_sz));
    xsetopt(sfd, XRCVBUF, &buf_sz, sizeof(buf_sz));
    for (i = 0; i < cnt; i++) {
	nbytes = rand() % 1024;
	payload = xallocmsg(nbytes);
	memcpy(payload, buf, nbytes);
	ASSERT_EQ(0, xsend(sfd, payload));
	ASSERT_EQ(0, xrecv(sfd, &payload));
	assert(memcmp(payload, buf, nbytes) == 0);
	xfreemsg(payload);
    }
    xclose(sfd);
}

static int xclient_thread(void *arg) {
    xclient(*(int *)arg);
    return 0;
}

static void xserver(int pf) {
    int i;
    int buf_sz = 0;
    int afd, sfd;
    thread_t cli_thread = {};
    char *payload;

    ASSERT_TRUE((afd = xlisten(pf, "127.0.0.1:18894")) >= 0);
    thread_start(&cli_thread, xclient_thread, &pf);

    BUG_ON((sfd = xaccept(afd)) < 0);
    xsetopt(sfd, XSNDBUF, &buf_sz, sizeof(buf_sz));
    xsetopt(sfd, XRCVBUF, &buf_sz, sizeof(buf_sz));
    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE(0 == xrecv(sfd, &payload));
	ASSERT_TRUE(0 == xsend(sfd, payload));
    }
    xclose(afd);
}

static struct xpoll_t *po = 0;
static spin_t lock;

static void xclient2(int pf) {
    int i;
    int sfd[cnt];
    struct xpoll_event event[cnt];

    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE((sfd[i] = xconnect(pf, "127.0.0.1:18895")) >= 0);
	event[i].xd = sfd[i];
	event[i].self = po;
	event[i].care = XPOLLIN|XPOLLOUT|XPOLLERR;
	spin_lock(&lock);
	assert(xpoll_ctl(po, XPOLL_ADD, &event[i]) == 0);
	spin_unlock(&lock);
    }
    for (i = 0; i < cnt; i++)
	xclose(sfd[i]);
}

static int xclient_thread2(void *arg) {
    xclient2(*(int *)arg);
    return 0;
}

static void xserver2(int pf) {
    int i, mycnt;
    int afd, sfd[cnt];
    thread_t cli_thread = {};
    struct xpoll_event event[cnt];

    po = xpoll_create();
    spin_init(&lock);

    ASSERT_TRUE((afd = xlisten(pf, "127.0.0.1:18895")) >= 0);
    thread_start(&cli_thread, xclient_thread2, &pf);
    for (i = 0; i < cnt; i++) {
	BUG_ON((sfd[i] = xaccept(afd)) < 0);
	event[i].xd = sfd[i];
	event[i].self = po;
	event[i].care = XPOLLIN|XPOLLOUT|XPOLLERR;
	spin_lock(&lock);
	assert(xpoll_ctl(po, XPOLL_ADD, &event[i]) == 0);
	spin_unlock(&lock);
    }
    mycnt = rand() % (cnt);
    for (i = 0; i < mycnt; i++) {
	event[i].xd = sfd[i];
	event[i].self = po;
	event[i].care = XPOLLIN|XPOLLOUT|XPOLLERR;
	spin_lock(&lock);
	assert(xpoll_ctl(po, XPOLL_DEL, &event[i]) == 0);
	assert(xpoll_ctl(po, XPOLL_DEL, &event[i]) == -1);
	spin_unlock(&lock);
    }
    xpoll_close(po);
    for (i = 0; i < cnt; i++)
	xclose(sfd[i]);

    thread_stop(&cli_thread);
    spin_destroy(&lock);
    xclose(afd);
}


TEST(xsock, vf) {
    xserver(PF_NET);
    xserver(PF_IPC);
    //xserver(PF_INPROC);
    
    xserver2(PF_NET);
    xserver2(PF_IPC);
    //xserver2(PF_INPROC);
}





static int cnt2 = 100;

static void inproc_client2() {
    int sfd, i;

    for (i = 0; i < cnt2/2; i++) {
	if ((sfd = xconnect(PF_INPROC, "/b_inproc")) < 0)
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
	if ((sfd = xconnect(PF_INPROC, "/b_inproc")) < 0)
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

    ASSERT_TRUE((afd = xlisten(PF_INPROC, "/b_inproc")) >= 0);
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
    //inproc_server_thread2();
}

