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

static void tcp_client() {
    int sfd, i;
    int64_t nbytes;
    char buf[1024] = {};
    struct channel_msg *msg;

    randstr(buf, 1024);
    ASSERT_TRUE((sfd = channel_connect(PF_NET, "127.0.0.1:18894")) >= 0);
    for (i = 0; i < cnt; i++) {
	nbytes = rand() % 1024;
	msg = channel_allocmsg(nbytes, nbytes);
	memcpy(msg->payload, buf, nbytes);
	memcpy(msg->control, buf, nbytes);
	ASSERT_EQ(0, channel_send(sfd, msg));
	ASSERT_EQ(0, channel_recv(sfd, &msg));
	channel_freemsg(msg);
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
    struct channel_msg *msg;


    ASSERT_TRUE((afd = channel_listen(PF_NET, "*:18894")) >= 0);
    thread_start(&cli_thread, tcp_client_thread, NULL);
    ASSERT_TRUE((sfd = channel_accept(afd)) >= 0);
    
    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE(0 == channel_recv(sfd, &msg));
	ASSERT_TRUE(0 == channel_send(sfd, msg));
    }
    ASSERT_TRUE((sfd2 = channel_accept(afd)) >= 0);
    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE(0 == channel_recv(sfd2, &msg));
	ASSERT_TRUE(0 == channel_send(sfd2, msg));
    }
    channel_close(sfd);
    channel_close(sfd2);
    ASSERT_TRUE((sfd2 = channel_accept(afd)) >= 0);
    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE(0 == channel_recv(sfd2, &msg));
	ASSERT_TRUE(0 == channel_send(sfd2, msg));
    }
    thread_stop(&cli_thread);
    channel_close(sfd2);
    channel_close(afd);
}

TEST(channel, msg) {
    struct channel_msg *msg = channel_allocmsg(1024, 1024);
    channel_freemsg(msg);
}

TEST(channel, tcp) {
    tcp_server_thread();
}



static void inproc_client() {
    int sfd, i;
    int64_t nbytes;
    char buf[1024] = {};
    struct channel_msg *msg;

    randstr(buf, 1024);
    ASSERT_TRUE((sfd = channel_connect(PF_INPROC, "/a_inproc")) >= 0);
    for (i = 0; i < cnt; i++) {
	nbytes = rand() % 1024;
	msg = channel_allocmsg(nbytes, nbytes);
	memcpy(msg->payload, buf, nbytes);
	memcpy(msg->control, buf, nbytes);
	ASSERT_EQ(0, channel_send(sfd, msg));
	ASSERT_EQ(0, channel_recv(sfd, &msg));
	channel_freemsg(msg);
    }
    channel_close(sfd);
}

static int inproc_client_thread(void *arg) {
    inproc_client();
    inproc_client();
    inproc_client();
    return 0;
}

static void inproc_server_thread() {
    int i;
    int afd, sfd, sfd2;
    thread_t cli_thread = {};
    struct channel_msg *msg;


    ASSERT_TRUE((afd = channel_listen(PF_INPROC, "/a_inproc")) >= 0);
    thread_start(&cli_thread, inproc_client_thread, NULL);

    while ((sfd = channel_accept(afd)) < 0)
	usleep(1000);
    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE(0 == channel_recv(sfd, &msg));
	ASSERT_TRUE(0 == channel_send(sfd, msg));
    }

    while ((sfd2 = channel_accept(afd)) < 0)
	usleep(1000);
    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE(0 == channel_recv(sfd2, &msg));
	ASSERT_TRUE(0 == channel_send(sfd2, msg));
    }
    channel_close(sfd);
    channel_close(sfd2);

    while ((sfd2 = channel_accept(afd)) < 0)
	usleep(1000);
    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE(0 == channel_recv(sfd2, &msg));
	ASSERT_TRUE(0 == channel_send(sfd2, msg));
    }

    thread_stop(&cli_thread);
    channel_close(sfd2);
    channel_close(afd);
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
    inproc_server_thread();
    inproc_server_thread2();
}

