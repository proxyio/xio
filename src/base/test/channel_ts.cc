#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
extern "C" {
#include <sync/spin.h>
#include <channel/channel.h>
#include <runner/thread.h>
}

extern int randstr(char *buf, int len);

static int cnt = 10;
static int pf;

static void channel_client() {
    int sfd, i;
    int buf_sz = 0;
    int64_t nbytes;
    char buf[1024] = {};
    char *payload;

    randstr(buf, 1024);
    ASSERT_TRUE((sfd = channel_connect(pf, "127.0.0.1:18894")) >= 0);
    channel_setopt(sfd, CHANNEL_SNDBUF, &buf_sz, sizeof(buf_sz));
    channel_setopt(sfd, CHANNEL_RCVBUF, &buf_sz, sizeof(buf_sz));
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

static int channel_client_thread(void *arg) {
    channel_client();
    channel_client();
    channel_client();
    return 0;
}

static void channel_server_thread() {
    int i;
    int buf_sz = 0;
    int afd, sfd, sfd2;
    thread_t cli_thread = {};
    char *payload;


    ASSERT_TRUE((afd = channel_listen(pf, "127.0.0.1:18894")) >= 0);
    thread_start(&cli_thread, channel_client_thread, NULL);

    while ((sfd = channel_accept(afd)) < 0)
	usleep(10000);
    channel_setopt(sfd, CHANNEL_SNDBUF, &buf_sz, sizeof(buf_sz));
    channel_setopt(sfd, CHANNEL_RCVBUF, &buf_sz, sizeof(buf_sz));
    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE(0 == channel_recv(sfd, &payload));
	ASSERT_TRUE(0 == channel_send(sfd, payload));
    }
    while ((sfd2 = channel_accept(afd)) < 0)
	usleep(10000);
    channel_setopt(sfd2, CHANNEL_SNDBUF, &buf_sz, sizeof(buf_sz));
    channel_setopt(sfd2, CHANNEL_RCVBUF, &buf_sz, sizeof(buf_sz));
    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE(0 == channel_recv(sfd2, &payload));
	ASSERT_TRUE(0 == channel_send(sfd2, payload));
    }
    channel_close(sfd);
    channel_close(sfd2);
    while ((sfd2 = channel_accept(afd)) < 0)
	usleep(10000);
    channel_setopt(sfd2, CHANNEL_SNDBUF, &buf_sz, sizeof(buf_sz));
    channel_setopt(sfd2, CHANNEL_RCVBUF, &buf_sz, sizeof(buf_sz));
    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE(0 == channel_recv(sfd2, &payload));
	ASSERT_TRUE(0 == channel_send(sfd2, payload));
    }
    thread_stop(&cli_thread);
    channel_close(sfd2);
    channel_close(afd);
}


struct upoll_table *ut;
spin_t lock;

static void channel_client2() {
    int i;
    int sfd[cnt];
    struct upoll_event event[cnt];

    for (i = 0; i < cnt; i++) {
	ASSERT_TRUE((sfd[i] = channel_connect(pf, "127.0.0.1:18895")) >= 0);
	event[i].cd = sfd[i];
	event[i].self = ut;
	event[i].care = UPOLLIN|UPOLLOUT|UPOLLERR;
	spin_lock(&lock);
	assert(upoll_ctl(ut, UPOLL_ADD, &event[i]) == 0);
	spin_unlock(&lock);
    }
    for (i = 0; i < cnt; i++)
	channel_close(sfd[i]);
}

static int channel_client_thread2(void *arg) {
    channel_client2();
    return 0;
}

static void channel_server_thread2() {
    int i, mycnt;
    int afd, sfd[cnt];
    thread_t cli_thread = {};
    struct upoll_event event[cnt];

    ut = upoll_create();
    spin_init(&lock);

    ASSERT_TRUE((afd = channel_listen(pf, "127.0.0.1:18895")) >= 0);
    thread_start(&cli_thread, channel_client_thread2, NULL);
    for (i = 0; i < cnt; i++) {
	while ((sfd[i] = channel_accept(afd)) < 0)
	    usleep(10000);
	event[i].cd = sfd[i];
	event[i].self = ut;
	event[i].care = UPOLLIN|UPOLLOUT|UPOLLERR;
	spin_lock(&lock);
	assert(upoll_ctl(ut, UPOLL_ADD, &event[i]) == 0);
	spin_unlock(&lock);
    }
    mycnt = rand() % cnt;
    for (i = 0; i < 0; i++) {
	event[i].cd = sfd[i];
	event[i].self = ut;
	event[i].care = UPOLLIN|UPOLLOUT|UPOLLERR;
	spin_lock(&lock);
	assert(upoll_ctl(ut, UPOLL_DEL, &event[i]) == 0);
	assert(upoll_ctl(ut, UPOLL_DEL, &event[i]) == -1);
	spin_unlock(&lock);
    }
    for (i = 0; i < cnt; i++)
	channel_close(sfd[i]);
    thread_stop(&cli_thread);
    spin_destroy(&lock);
    upoll_close(ut);
    channel_close(afd);
}

TEST(channel, vf) {
    pf = PF_NET;
    channel_server_thread();
    channel_server_thread2();
    pf = PF_INPROC;
    channel_server_thread();
    //channel_server_thread2();
    pf = PF_IPC;
    channel_server_thread();
    channel_server_thread2();
}





static int cnt2 = 100;

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

