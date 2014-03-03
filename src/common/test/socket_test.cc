#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
extern "C" {
#include "net/socket.h"
#include "net/accepter.h"
#include "runner/thread.h"
}

extern int randstr(char *buf, int len);

static void tcp_client() {
    int sfd;
    int64_t nbytes;
    char buf[1024] = {};

    ASSERT_TRUE((sfd = sk_connect("tcp", "", "127.0.0.1:18894")) > 0);
    randstr(buf, 1024);
    EXPECT_EQ(500, nbytes = sk_write(sfd, buf, 500));
    EXPECT_EQ(nbytes, sk_read(sfd, buf, nbytes));

    ASSERT_EQ(0, sk_reconnect(&sfd));
    EXPECT_EQ(500, nbytes = sk_write(sfd, buf, 500));
    EXPECT_EQ(nbytes, sk_read(sfd, buf, nbytes));
    close(sfd);
}

static int client_thread(void *arg) {
    tcp_client();
    return 0;
}

static void server_thread() {
    int afd, sfd;
    char buf[1024];
    int64_t nbytes;
    thread_t cli_thread = {};

    ASSERT_TRUE((afd = act_listen("tcp", "*:18894", 100)) > 0);
    thread_start(&cli_thread, client_thread, NULL);
    ASSERT_TRUE((sfd = act_accept(afd)) > 0);
    nbytes = sk_read(sfd, buf, 1024);
    EXPECT_EQ(nbytes, sk_write(sfd, buf, nbytes));
    close(sfd);

    ASSERT_TRUE((sfd = act_accept(afd)) > 0);
    nbytes = sk_read(sfd, buf, 1024);
    EXPECT_EQ(nbytes, sk_write(sfd, buf, nbytes));
    close(sfd);

    close(afd);
    thread_stop(&cli_thread);
}

TEST(net, socket) {
    server_thread();
}
