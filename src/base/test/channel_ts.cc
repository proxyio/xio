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
    channel_close(sfd);
}

static int channel_client_thread(void *arg) {
    channel_client();
    channel_client();
    return 0;
}

static void channel_server_thread() {
    int i;
    int afd, sfd, sfd2;
    thread_t cli_thread = {};
    struct channel_msg *msg;


    ASSERT_TRUE((afd = channel_listen(PIO_TCP, "*:18894")) >= 0);
    thread_start(&cli_thread, channel_client_thread, NULL);
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
    thread_stop(&cli_thread);
    channel_close(sfd);
    channel_close(afd);
    channel_close(sfd2);
}

TEST(transport, channel) {
    channel_server_thread();
}
