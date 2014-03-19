#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
extern "C" {
#include "os/epoll.h"
#include "transport/tcp/tcp.h"
#include "runner/thread.h"
}

extern int randstr(char *buf, int len);

static void tcp_client() {
    int sfd;
    int64_t nbytes;
    char buf[1024] = {};

    ASSERT_TRUE((sfd = tcp_connect("127.0.0.1:18894")) > 0);
    randstr(buf, 1024);
    EXPECT_EQ(sizeof(buf), nbytes = tcp_write(sfd, buf, sizeof(buf)));
    EXPECT_EQ(nbytes, tcp_read(sfd, buf, nbytes));

    ASSERT_EQ(0, tcp_reconnect(&sfd));
    EXPECT_EQ(sizeof(buf), nbytes = tcp_write(sfd, buf, sizeof(buf)));
    EXPECT_EQ(nbytes, tcp_read(sfd, buf, nbytes));
    close(sfd);
}

static int client_thread(void *arg) {
    tcp_client();
    return 0;
}

int client_event_handler(struct epoll *el, epollevent_t *et) {
    char buf[1024] = {};

    randstr(buf, sizeof(buf));
    if (et->happened & EPOLLIN) {
	EXPECT_EQ(sizeof(buf), tcp_read(et->fd, buf, sizeof(buf)));
	EXPECT_EQ(sizeof(buf), tcp_write(et->fd, buf, sizeof(buf)));
    }
    if (et->happened & EPOLLRDHUP) {
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

    ASSERT_TRUE((afd = tcp_listen("*:18894")) > 0);
    thread_start(&cli_thread, client_thread, NULL);

    ASSERT_TRUE((sfd = tcp_accept(afd)) > 0);
    et.f = client_event_handler;
    et.fd = sfd;
    et.events = EPOLLIN|EPOLLRDHUP;

    epoll_add(&el, &et);
    epoll_oneloop(&el);

    epoll_del(&el, &et);
    close(sfd);
    ASSERT_TRUE((sfd = tcp_accept(afd)) > 0);
    et.fd = sfd;
    epoll_add(&el, &et);
    epoll_oneloop(&el);

    thread_stop(&cli_thread);
    epoll_destroy(&el);
}

TEST(net, socket) {
    server_thread();
}
