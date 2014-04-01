#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
extern "C" {
#include "os/eventloop.h"
#include "transport/tcp/tcp.h"
#include "transport/ipc/ipc.h"
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
    close(sfd);
}

static int tcp_client_thread(void *arg) {
    tcp_client();
    tcp_client();
    return 0;
}

int tcp_client_event_handler(eloop_t *el, ev_t *et) {
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


static void tcp_server_thread() {
    int afd, sfd;
    thread_t cli_thread = {};
    eloop_t el = {};
    ev_t et = {};
    
    epoll_init(&el, 1024, 100, 10);

    ASSERT_TRUE((afd = tcp_bind("*:18894")) > 0);
    thread_start(&cli_thread, tcp_client_thread, NULL);

    ASSERT_TRUE((sfd = tcp_accept(afd)) > 0);
    et.f = tcp_client_event_handler;
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
    close(sfd);

    close(afd);
}


static void ipc_client() {
    int sfd;
    int64_t nbytes;
    char buf[1024] = {};

    if ((sfd = ipc_connect("/tmp/pio_ipc_socket")) < 0)
	ASSERT_TRUE(0);
    randstr(buf, 1024);
    EXPECT_EQ(sizeof(buf), nbytes = ipc_write(sfd, buf, sizeof(buf)));
    EXPECT_EQ(nbytes, ipc_read(sfd, buf, nbytes));
    close(sfd);
}

static int ipc_client_thread(void *arg) {
    ipc_client();
    ipc_client();
    return 0;
}

int ipc_client_event_handler(eloop_t *el, ev_t *et) {
    char buf[1024] = {};

    randstr(buf, sizeof(buf));
    if (et->happened & EPOLLIN) {
	EXPECT_EQ(sizeof(buf), ipc_read(et->fd, buf, sizeof(buf)));
	EXPECT_EQ(sizeof(buf), ipc_write(et->fd, buf, sizeof(buf)));
    }
    if (et->happened & EPOLLRDHUP) {
	epoll_del(el, et);
	close(et->fd);
    }
    return 0;
}


static void ipc_server_thread() {
    int afd, sfd;
    thread_t cli_thread = {};
    eloop_t el = {};
    ev_t et = {};
    
    epoll_init(&el, 1024, 100, 10);

    ASSERT_TRUE((afd = ipc_bind("/tmp/pio_ipc_socket")) > 0);
    thread_start(&cli_thread, ipc_client_thread, NULL);
    ASSERT_TRUE((sfd = ipc_accept(afd)) > 0);
    et.f = ipc_client_event_handler;
    et.fd = sfd;
    et.events = EPOLLIN|EPOLLRDHUP;

    epoll_add(&el, &et);
    epoll_oneloop(&el);

    epoll_del(&el, &et);
    close(sfd);
    ASSERT_TRUE((sfd = ipc_accept(afd)) > 0);
    et.fd = sfd;
    epoll_add(&el, &et);
    epoll_oneloop(&el);
    thread_stop(&cli_thread);
    epoll_destroy(&el);
    close(sfd);

    close(afd);
}

TEST(transport, ipc) {
    ipc_server_thread();
}

TEST(transport, tcp) {
    tcp_server_thread();
}
