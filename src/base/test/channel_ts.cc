#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
extern "C" {
#include "channel/channel.h"
#include "runner/thread.h"
}

extern int randstr(char *buf, int len);

static int cnt = 10;
static int pf;

static void tcp_client() {
    int sfd, i;
    int64_t nbytes;
    char buf[1024] = {};
    char *payload;

    randstr(buf, 1024);
    ASSERT_TRUE((sfd = channel_connect(pf, "127.0.0.1:18894")) >= 0);
    for (i = 0; i < cnt; i++) {
	nbytes = rand() % 1024;
	payload = channel_allocmsg(nbytes);
	memcpy(payload, buf, nbytes);
	ASSERT_EQ(0, channel_send(sfd, payload));
	ASSERT_EQ(0, channel_recv(sfd, &payload));
	assert(memcmp(payload, buf, nbytes) == 0);
	channel_freemsg(payload);
    }
    channel_close(sfd);
}

static int tcp_client_thread(void *arg) {
    tcp_client();
    tcp_client();
    tcp_client();
    return 0;
}

static void tcp_server_thread() {
    int i;
    int afd, sfd, sfd2;
    thread_t cli_thread = {};
    char *payload;


    ASSERT_TRUE((afd = channel_listen(pf, "127.0.0.1:18894")) >= 0);
    thread_start(&cli_thread, tcp_client_thread, NULL);

    while ((sfd = channel_accept(afd)) < 0)
	usleep(1000);
    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE(0 == channel_recv(sfd, &payload));
	ASSERT_TRUE(0 == channel_send(sfd, payload));
    }
    while ((sfd2 = channel_accept(afd)) < 0)
	usleep(1000);
    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE(0 == channel_recv(sfd2, &payload));
	ASSERT_TRUE(0 == channel_send(sfd2, payload));
    }
    channel_close(sfd);
    channel_close(sfd2);
    while ((sfd2 = channel_accept(afd)) < 0)
	usleep(1000);
    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE(0 == channel_recv(sfd2, &payload));
	ASSERT_TRUE(0 == channel_send(sfd2, payload));
    }
    thread_stop(&cli_thread);
    channel_close(sfd2);
    channel_close(afd);
}

TEST(channel, vf) {
    pf = PF_NET;
    tcp_server_thread();
    pf = PF_INPROC;
    tcp_server_thread();
    pf = PF_IPC;
    tcp_server_thread();
}

static int cnt2 = 1000;

static void inproc_client2() {
    int sfd, i;

    for (i = 0; i < cnt2/2; i++) {
	if ((sfd = channel_connect(PF_INPROC, "/b_inproc")) < 0)
	    break;
	channel_close(sfd);
    }
}

static int inproc_client_thread2(void *args) {
    inproc_client2();
    return 0;
}

static void inproc_client3() {
    int sfd, i;

    for (i = 0; i < cnt2/2; i++) {
	if ((sfd = channel_connect(PF_INPROC, "/b_inproc")) < 0)
	    break;
	channel_close(sfd);
    }
}

static int inproc_client_thread3(void *args) {
    inproc_client3();
    return 0;
}

static void inproc_server_thread2() {
    int i, afd, sfd;
    thread_t cli_thread[2] = {};

    ASSERT_TRUE((afd = channel_listen(PF_INPROC, "/b_inproc")) >= 0);
    thread_start(&cli_thread[0], inproc_client_thread2, NULL);
    thread_start(&cli_thread[1], inproc_client_thread3, NULL);

    for (i = 0; i < cnt2 - 10; i++) {
	while ((sfd = channel_accept(afd)) < 0)
	    usleep(1000);
	channel_close(sfd);
    }
    channel_close(afd);
    thread_stop(&cli_thread[0]);
    thread_stop(&cli_thread[1]);
}

TEST(channel, inproc) {
    inproc_server_thread2();
}

