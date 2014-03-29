#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
extern "C" {
#include "transport/channel.h"
#include "transport/channel_poller.h"
#include "runner/thread.h"
}

extern int randstr(char *buf, int len);

static int cnt = 1;

static void channel_client() {
    int sfd, i;
    int64_t nbytes;
    char buf[1024] = {};
    struct channel_msg *msg;

    randstr(buf, 1024);
    ASSERT_TRUE((sfd = channel_connect(PIO_TCP, "127.0.0.1:18894")) >= 0);
    for (i = 0; i < cnt; i++) {
	nbytes = rand() % 1024;
	msg = channel_allocmsg(nbytes, nbytes);
	memcpy(msg->payload, buf, nbytes);
	memcpy(msg->control, buf, nbytes);
	ASSERT_EQ(0, channel_send(sfd, msg));
	channel_freemsg(msg);
	ASSERT_EQ(0, channel_recv(sfd, &msg));
	channel_freemsg(msg);
    }
    close(sfd);
}

static int channel_client_thread(void *arg) {
    channel_client();
    channel_client();
    return 0;
}

static void channel_server_thread() {
    int afd, sfd, poller;
    thread_t cli_thread = {};
    struct channel_msg *msg;
    struct channel_event et = {}, tmp = {};

    ASSERT_TRUE((poller = channelpoller_create()) >= 0);
    ASSERT_TRUE((afd = channel_listen(PIO_TCP, "*:18894")) >= 0);
    thread_start(&cli_thread, channel_client_thread, NULL);

    ASSERT_TRUE((sfd = channel_accept(afd)) >= 0);
    et.events = CHANNEL_MSGOUT;
    et.ptr = &et;
    channelpoller_setev(poller, sfd, &et);
    channelpoller_getev(poller, sfd, &tmp);
    ASSERT_TRUE(memcmp(&et, &tmp, sizeof(tmp) == 0));
    ASSERT_TRUE(1 == channelpoller_wait(poller, &tmp, 1));
    ASSERT_TRUE(tmp.events == CHANNEL_MSGOUT);
    ASSERT_TRUE(1 == channelpoller_wait(poller, &tmp, 1));
    ASSERT_TRUE(tmp.events == CHANNEL_MSGOUT);
    et.events = 0;
    et.ptr = &et;
    channelpoller_setev(poller, sfd, &et);
    ASSERT_TRUE(0 == channelpoller_wait(poller, &tmp, 1));
    et.events = CHANNEL_MSGIN;
    et.ptr = &et;
    channelpoller_setev(poller, sfd, &et);
    ASSERT_TRUE(1 == channelpoller_wait(poller, &tmp, 1));
    ASSERT_TRUE(0 == channel_recv(sfd, &msg));
    ASSERT_TRUE(0 == channel_send(sfd, msg));

    channelpoller_close(poller);
    channel_close(afd);
    channel_close(sfd);
}

TEST(transport, channel) {
    channel_global_init();
    channel_server_thread();
}
