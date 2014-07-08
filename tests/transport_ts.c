#include <errno.h>
#include <time.h>
#include <string.h>
#include <utils/waitgroup.h>
#include <utils/eventloop.h>
#include <utils/tcp/tcp.h>
#include <utils/ipc/ipc.h>
#include <utils/thread.h>
#include "testutil.h"

/* Compatiple for HAVE_DEBUG flags */
static int64_t tcp_send_full (int fd, char *buff, int len)
{
	int rc = 0;
	int full = len;

	while (len > 0) {
		if ((rc = tcp_send (fd, buff, len)) >= 0) {
			buff += rc;
			len -= rc;
		} else
			break;
	}
	return full - len;
}

static int64_t tcp_recv_full (int fd, char *buff, int len)
{
	int rc = 0;
	int full = len;

	while (len > 0) {
		if ((rc = tcp_recv (fd, buff, len)) >= 0) {
			buff += rc;
			len -= rc;
		} else
			break;
	}
	return full - len;
}


static void tcp_client()
{
	int sfd;
	int64_t nbytes;
	char buf[1024] = {};

	BUG_ON ( (sfd = tcp_connect ("127.0.0.1:15100") ) <= 0);
	randstr (buf, 1024);
	BUG_ON (sizeof (buf) != (nbytes = tcp_send_full (sfd, buf, sizeof (buf) ) ) );
	BUG_ON (nbytes != tcp_recv_full (sfd, buf, nbytes) );
	close (sfd);
}

static int tcp_client_thread (void *arg)
{
	tcp_client();
	tcp_client();
	return 0;
}

int tcp_client_event_handler (eloop_t *el, ev_t *et)
{
	char buf[1024] = {};

	randstr (buf, sizeof (buf) );
	if (et->happened & EPOLLIN) {
		BUG_ON (sizeof (buf) != tcp_recv_full (et->fd, buf, sizeof (buf) ) );
		BUG_ON (sizeof (buf) != tcp_send_full (et->fd, buf, sizeof (buf) ) );
	}
	if (et->happened & EPOLLRDHUP) {
		eloop_del (el, et);
		close (et->fd);
	}
	return 0;
}


static void tcp_server_thread()
{
	int afd, sfd;
	thread_t cli_thread = {};
	eloop_t el = {};
	ev_t et = {};

	eloop_init (&el, 1024, 100, 10);

	BUG_ON ( (afd = tcp_bind ("*:15100") ) <= 0);
	thread_start (&cli_thread, tcp_client_thread, NULL);

	BUG_ON ( (sfd = tcp_accept (afd) ) <= 0);
	et.f = tcp_client_event_handler;
	et.fd = sfd;
	et.events = EPOLLIN|EPOLLRDHUP;

	eloop_add (&el, &et);
	eloop_once (&el);

	eloop_del (&el, &et);
	close (sfd);
	BUG_ON ( (sfd = tcp_accept (afd) ) <= 0);
	et.fd = sfd;
	eloop_add (&el, &et);
	eloop_once (&el);
	thread_stop (&cli_thread);
	eloop_destroy (&el);
	close (sfd);

	close (afd);
}


static void ipc_client()
{
	int sfd;
	int64_t nbytes;
	char buf[1024] = {};
	char *sockaddr = "pio_ipc_socket";

	if ( (sfd = ipc_connect (sockaddr) ) < 0)
		BUG_ON (1);
	randstr (buf, 1024);
	BUG_ON (sizeof (buf) != (nbytes = ipc_send (sfd, buf, sizeof (buf) ) ) );
	BUG_ON (nbytes != ipc_recv (sfd, buf, nbytes) );
	close (sfd);
}

static int ipc_client_thread (void *arg)
{
	ipc_client();
	ipc_client();
	return 0;
}

int ipc_client_event_handler (eloop_t *el, ev_t *et)
{
	char buf[1024] = {};

	randstr (buf, sizeof (buf) );
	if (et->happened & EPOLLIN) {
		BUG_ON (sizeof (buf) != ipc_recv (et->fd, buf, sizeof (buf) ) );
		BUG_ON (sizeof (buf) != ipc_send (et->fd, buf, sizeof (buf) ) );
	}
	if (et->happened & EPOLLRDHUP) {
		eloop_del (el, et);
		close (et->fd);
	}
	return 0;
}


static void ipc_server_thread()
{
	int afd, sfd;
	thread_t cli_thread = {};
	eloop_t el = {};
	ev_t et = {};
	char *sockaddr = "pio_ipc_socket";

	eloop_init (&el, 1024, 100, 10);

	BUG_ON ( (afd = ipc_bind (sockaddr) ) <= 0);
	thread_start (&cli_thread, ipc_client_thread, NULL);
	BUG_ON ( (sfd = ipc_accept (afd) ) <= 0);
	et.f = ipc_client_event_handler;
	et.fd = sfd;
	et.events = EPOLLIN|EPOLLRDHUP;

	eloop_add (&el, &et);
	eloop_once (&el);

	eloop_del (&el, &et);
	close (sfd);
	BUG_ON ( (sfd = ipc_accept (afd) ) <= 0);
	et.fd = sfd;
	eloop_add (&el, &et);
	eloop_once (&el);
	thread_stop (&cli_thread);
	eloop_destroy (&el);
	close (sfd);

	close (afd);
	unlink (sockaddr);
}

static void tcp_test_sock_opt (int sfd)
{
	int on;
	int optlen = 0;

	on = 1;
	BUG_ON (tcp_setopt (sfd, TP_NOBLOCK, &on, sizeof (on) ) );
	BUG_ON (tcp_getopt (sfd, TP_NOBLOCK, &on, &optlen) );
	BUG_ON (on != 1);

	on = 0;
	BUG_ON (tcp_setopt (sfd, TP_NOBLOCK, &on, sizeof (on) ) );
	BUG_ON (tcp_getopt (sfd, TP_NOBLOCK, &on, &optlen) );
	BUG_ON (on != 0);

	on = 99;
	BUG_ON (tcp_setopt (sfd, TP_SNDTIMEO, &on, sizeof (on) ) );
	BUG_ON (tcp_getopt (sfd, TP_SNDTIMEO, &on, &optlen) );
	BUG_ON (on != 99);

	on = 98;
	BUG_ON (tcp_setopt (sfd, TP_RCVTIMEO, &on, sizeof (on) ) );
	BUG_ON (tcp_getopt (sfd, TP_RCVTIMEO, &on, &optlen) );
	BUG_ON (on != 98);
}

static int can_exit = 0;

static int server_thread (void *args)
{
	waitgroup_t *wg = (waitgroup_t *) args;
	int afd = tcp_bind ("*:15100");
	int sfd;
	int on;
	int optlen = 0;

	on = 1;
	BUG_ON (tcp_setopt (afd, TP_NOBLOCK, &on, sizeof (on) ) );
	BUG_ON (tcp_getopt (afd, TP_NOBLOCK, &on, &optlen) );
	BUG_ON (on != 1);

	on = 0;
	BUG_ON (tcp_setopt (afd, TP_NOBLOCK, &on, sizeof (on) ) );
	BUG_ON (tcp_getopt (afd, TP_NOBLOCK, &on, &optlen) );
	BUG_ON (on != 0);

	BUG_ON (afd < 0);
	waitgroup_done (wg);
	sfd = tcp_accept (afd);
	BUG_ON (sfd < 0);
	tcp_test_sock_opt (sfd);

	while (!can_exit)
		usleep (20000);
	tcp_close (sfd);
	tcp_close (afd);
	return 0;
}

static void tcp_option()
{
	waitgroup_t wg;
	int sfd;
	thread_t t;

	waitgroup_init (&wg);
	waitgroup_add (&wg);
	thread_start (&t, server_thread, &wg);
	waitgroup_wait (&wg);

	sfd = tcp_connect ("127.0.0.1:15100");
	BUG_ON (sfd < 0);
	tcp_test_sock_opt (sfd);
	can_exit = 1;
	thread_stop (&t);
	tcp_close (sfd);
}

static void ipc_option()
{
}

static void tcp_sg_send()
{
}


int main (int argc, char **argv)
{
	int sfd = tcp_connect ("127.0.0.1:15199");
	BUG_ON (sfd >= 0);

	ipc_server_thread();
	tcp_server_thread();
	tcp_option();
	ipc_option();
	return 0;
}
