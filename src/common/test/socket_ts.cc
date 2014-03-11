#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
extern "C" {
#include "os/epoll.h"
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
    EXPECT_EQ(sizeof(buf), nbytes = sk_write(sfd, buf, sizeof(buf)));
    EXPECT_EQ(nbytes, sk_read(sfd, buf, nbytes));

    ASSERT_EQ(0, sk_reconnect(&sfd));
    EXPECT_EQ(sizeof(buf), nbytes = sk_write(sfd, buf, sizeof(buf)));
    EXPECT_EQ(nbytes, sk_read(sfd, buf, nbytes));
    close(sfd);
}

static int client_thread(void *arg) {
    tcp_client();
    return 0;
}

int client_event_handler(struct epoll *el, epollevent_t *et, uint32_t happened) {
    char buf[1024] = {};

    randstr(buf, sizeof(buf));
    if (happened & EPOLLIN) {
	EXPECT_EQ(sizeof(buf), sk_read(et->fd, buf, sizeof(buf)));
	EXPECT_EQ(sizeof(buf), sk_write(et->fd, buf, sizeof(buf)));
    }
    if (happened & EPOLLRDHUP) {
	epoll_del(el, et);
	close(et->fd);
    }
    return 0;
}


static void server_thread() {
    int afd, sfd;
    thread_t cli_thread = {};
    epoll_t el = {};
    epollevent_t et = {};
    
    epoll_init(&el, 1024, 100, 10);

    ASSERT_TRUE((afd = act_listen("tcp", "*:18894", 100)) > 0);
    thread_start(&cli_thread, client_thread, NULL);

    ASSERT_TRUE((sfd = act_accept(afd)) > 0);
    et.f = client_event_handler;
    et.fd = sfd;
    et.events = EPOLLIN|EPOLLRDHUP;

    epoll_add(&el, &et);
    epoll_oneloop(&el);

    epoll_del(&el, &et);
    close(sfd);
    ASSERT_TRUE((sfd = act_accept(afd)) > 0);
    et.fd = sfd;
    epoll_add(&el, &et);
    epoll_oneloop(&el);

    thread_stop(&cli_thread);
    epoll_destroy(&el);
}

TEST(net, socket) {
    server_thread();
}
