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
    xclient(PF_NET);
    xclient(PF_IPC);
    xclient(PF_INPROC);
    return 0;
}

static void xserver_thread() {
    int i;
    int buf_sz = 0;
    int afd, sfd, sfd2;
    thread_t cli_thread = {};
    char *payload;


    ASSERT_TRUE((afd = xlisten(PF_NET|PF_IPC|PF_INPROC, "127.0.0.1:18894")) >= 0);
    thread_start(&cli_thread, xclient_thread, NULL);

    while ((sfd = xaccept(afd)) < 0)
	usleep(10000);
    xsetopt(sfd, XSNDBUF, &buf_sz, sizeof(buf_sz));
    xsetopt(sfd, XRCVBUF, &buf_sz, sizeof(buf_sz));
    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE(0 == xrecv(sfd, &payload));
	ASSERT_TRUE(0 == xsend(sfd, payload));
    }
    while ((sfd2 = xaccept(afd)) < 0)
	usleep(10000);
    xsetopt(sfd2, XSNDBUF, &buf_sz, sizeof(buf_sz));
    xsetopt(sfd2, XRCVBUF, &buf_sz, sizeof(buf_sz));
    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE(0 == xrecv(sfd2, &payload));
	ASSERT_TRUE(0 == xsend(sfd2, payload));
    }
    xclose(sfd);
    xclose(sfd2);
    while ((sfd2 = xaccept(afd)) < 0)
	usleep(10000);
    xsetopt(sfd2, XSNDBUF, &buf_sz, sizeof(buf_sz));
    xsetopt(sfd2, XRCVBUF, &buf_sz, sizeof(buf_sz));
    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE(0 == xrecv(sfd2, &payload));
	ASSERT_TRUE(0 == xsend(sfd2, payload));
    }
    thread_stop(&cli_thread);
    xclose(sfd2);
    xclose(afd);
}


struct xpoll_t *po;
spin_t lock;

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
    xclient2(PF_IPC);
    xclient2(PF_NET);
    xclient2(PF_INPROC);
    return 0;
}

static void xserver_thread2() {
    int i, mycnt;
    int afd, sfd[3 * cnt];
    thread_t cli_thread = {};
    struct xpoll_event event[3 * cnt];

    po = xpoll_create();
    spin_init(&lock);

    ASSERT_TRUE((afd = xlisten(PF_NET|PF_IPC|PF_INPROC, "127.0.0.1:18895")) >= 0);
    thread_start(&cli_thread, xclient_thread2, NULL);
    for (i = 0; i < 3 * cnt; i++) {
	while ((sfd[i] = xaccept(afd)) < 0) {
	    usleep(10000);
	}
	event[i].xd = sfd[i];
	event[i].self = po;
	event[i].care = XPOLLIN|XPOLLOUT|XPOLLERR;
	spin_lock(&lock);
	assert(xpoll_ctl(po, XPOLL_ADD, &event[i]) == 0);
	spin_unlock(&lock);
    }
    mycnt = rand() % (3 * cnt);
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
    for (i = 0; i < 3 * cnt; i++)
	xclose(sfd[i]);

    thread_stop(&cli_thread);
    spin_destroy(&lock);
    xclose(afd);
}

TEST(xsock, vf) {
    xserver_thread();
    xserver_thread2();
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
	while ((sfd = xaccept(afd)) < 0)
	    usleep(1000);
	xclose(sfd);
    }
    xclose(afd);
    thread_stop(&cli_thread[0]);
    thread_stop(&cli_thread[1]);
}

TEST(xsock, inproc) {
    inproc_server_thread2();
}

