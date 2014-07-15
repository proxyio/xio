#include <errno.h>
#include <time.h>
#include <string.h>
#include <utils/spinlock.h>
#include <utils/thread.h>
#include <xio/poll.h>
#include <xio/socket.h>
#include <xio/cmsghdr.h>
#include "testutil.h"

#define cnt 3

static void xclient (const char *pf)
{
	int sfd, i, j;
	int64_t nbytes;
	char buf[1024] = {};
	char *xbuf, *oob;
	char host[1024];
	int debuglv = 3;

	sprintf (host, "%s%s", pf, "://127.0.0.1:15100");
	randstr (buf, 1024);
	BUG_ON ( (sfd = xconnect (host) ) < 0);
	//xsetopt (sfd, XL_SOCKET, XDEBUGON, &debuglv, sizeof (debuglv));
	for (i = 0; i < cnt; i++) {
		nbytes = rand() % 1024;
		for (j = 0; j < 10; j++) {
			xbuf = ubuf_alloc (nbytes);
			memcpy (xbuf, buf, nbytes);

			oob = ubuf_alloc (nbytes);
			memcpy (oob, buf, nbytes);

			ubufctl_add (xbuf, oob);
			BUG_ON (xsend (sfd, xbuf));
			DEBUG_OFF ("%d send request %d", sfd, j);
		}
		for (j = 0; j < 10; j++) {
			BUG_ON (0 != xrecv (sfd, &xbuf) );
			DEBUG_OFF ("%d recv response %d", sfd, j);
			BUG_ON (memcmp (xbuf, buf, nbytes) != 0);
			oob = ubufctl_first (xbuf);
			BUG_ON (memcmp (oob, buf, nbytes) != 0);
			ubuf_free (xbuf);
		}
	}
	xclose (sfd);
}

static int xclient_thread (void *arg)
{
	xclient ("tcp");
	xclient ("inproc");
	return 0;
}

static void xserver()
{
	int i, j;
	int afd, sfd;
	thread_t cli_thread = {};
	char *xbuf, *ubuf;
	char *host = "mix://tcp://127.0.0.1:15100+inproc://127.0.0.1:15100";

	BUG_ON ( (afd = xlisten (host) ) < 0);
	thread_start (&cli_thread, xclient_thread, 0);

	for (j = 0; j < 2; j++) {
		BUG_ON ( (sfd = xaccept (afd) ) < 0);
		DEBUG_OFF ("xserver accept %d", sfd);
		for (i = 0; i < cnt * 10; i++) {
			BUG_ON (0 != xrecv (sfd, &xbuf) );
			DEBUG_OFF ("%d recv", sfd);
			ubuf = ubuf_alloc (ubuf_len (xbuf));
			memcpy (ubuf, xbuf, ubuf_len (xbuf));
			ubufctl (xbuf, SSWITCH, ubuf);
			BUG_ON (0 != xsend (sfd, ubuf) );
			ubuf_free (xbuf);
		}
		xclose (sfd);
	}
	thread_stop (&cli_thread);
	DEBUG_OFF ("%s", "xclient thread return");
	xclose (afd);
}

static int pollid;

static void xclient2 (const char *pf)
{
	int i;
	int sfd[cnt];
	struct poll_fd ent[cnt] = {};
	char host[1024] = {};

	sprintf (host, "%s%s", pf, "://127.0.0.1:15200");
	for (i = 0; i < cnt; i++) {
		BUG_ON ( (sfd[i] = xconnect (host) ) < 0);
		ent[i].fd = sfd[i];
		ent[i].hndl = 0;
		ent[i].events = XPOLLIN|XPOLLOUT|XPOLLERR;
		assert (xpoll_ctl (pollid, XPOLL_ADD, &ent[i]) == 0);
	}
	for (i = 0; i < cnt; i++)
		xclose (sfd[i]);
}

static int xclient_thread2 (void *arg)
{
	xclient2 ("tcp");
	xclient2 ("ipc");
	xclient2 ("inproc");
	return 0;
}

static void xserver2()
{
	int i, j, mycnt;
	int afd, sfd[cnt];
	thread_t cli_thread = {};
	struct poll_fd ent[cnt] = {};

	pollid = xpoll_create();
	DEBUG_OFF ("%d", pollid);
	BUG_ON ( (afd = xlisten ("mix://tcp://127.0.0.1:15200+ipc://127.0.0.1:15200+inproc://127.0.0.1:15200") ) < 0);
	thread_start (&cli_thread, xclient_thread2, 0);
	ent[0].fd = afd;
	ent[0].hndl = 0;
	ent[0].events = XPOLLERR;
	BUG_ON (xpoll_ctl (pollid, XPOLL_ADD, &ent[0]) != 0);

	for (j = 0; j < 3; j++) {
		for (i = 0; i < cnt; i++) {
			BUG_ON ( (sfd[i] = xaccept (afd) ) < 0);
			DEBUG_OFF ("%d", sfd[i]);
			ent[i].fd = sfd[i];
			ent[i].hndl = 0;
			ent[i].events = XPOLLIN|XPOLLOUT|XPOLLERR;
			BUG_ON (xpoll_ctl (pollid, XPOLL_ADD, &ent[i]) != 0);
		}
		mycnt = rand() % (cnt);
		for (i = 0; i < mycnt; i++) {
			DEBUG_OFF ("%d", sfd[i]);
			ent[i].fd = sfd[i];
			ent[i].hndl = 0;
			ent[i].events = XPOLLIN|XPOLLOUT|XPOLLERR;
			BUG_ON (xpoll_ctl (pollid, XPOLL_MOD, &ent[i]) != 0);
			BUG_ON (xpoll_ctl (pollid, XPOLL_MOD, &ent[i]) != 0);
			BUG_ON (xpoll_ctl (pollid, XPOLL_DEL, &ent[i]) != 0);
			BUG_ON (xpoll_ctl (pollid, XPOLL_DEL, &ent[i]) != -1);
		}
		for (i = 0; i < cnt; i++)
			xclose (sfd[i]);
	}

	xpoll_close (pollid);
	thread_stop (&cli_thread);
	xclose (afd);
}

static void xsock_test (int count)
{
	while (count-- > 0) {
		xserver();
		DEBUG_OFF ("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
		//xserver2();
	}
}


#define cnt2 100

static void inproc_client2()
{
	int sfd, i;

	for (i = 0; i < cnt2/2; i++) {
		if ( (sfd = xconnect ("inproc://b_inproc") ) < 0) {
			BUG_ON (errno != ECONNREFUSED);
			continue;
		}
		xclose (sfd);
	}
}

static int inproc_client_thread2 (void *args)
{
	inproc_client2();
	return 0;
}

static void inproc_client3()
{
	int sfd, i;

	for (i = 0; i < cnt2/2; i++) {
		if ( (sfd = xconnect ("inproc://b_inproc") ) < 0) {
			BUG_ON (errno != ECONNREFUSED);
			continue;
		}
		xclose (sfd);
	}
}

static int inproc_client_thread3 (void *args)
{
	inproc_client3();
	return 0;
}

static void inproc_server_thread2()
{
	int i, afd, sfd;
	thread_t cli_thread[2] = {};

	BUG_ON ( (afd = xlisten ("inproc://b_inproc") ) < 0);
	thread_start (&cli_thread[0], inproc_client_thread2, NULL);
	thread_start (&cli_thread[1], inproc_client_thread3, NULL);

	for (i = 0; i < cnt2 - 10; i++) {
		BUG_ON ( (sfd = xaccept (afd) ) < 0);
		xclose (sfd);
	}
	xclose (afd);
	thread_stop (&cli_thread[0]);
	thread_stop (&cli_thread[1]);
}

static void xexp_test()
{
	inproc_server_thread2();
}



static void tcp_sg_send()
{
	int afd = xlisten ("tcp://127.0.0.1:15100");
	int cfd = xconnect ("tcp://127.0.0.1:15100");
	int i;
	char *ubuf;
	
	BUG_ON (afd < 0 || cfd < 0);
	for (i = 0; i < 1000; i++) {
		ubuf = ubuf_alloc (12);
		BUG_ON (xsend (cfd, ubuf));
	}
	xclose (afd);
	xclose (cfd);
}

int main (int argc, char **argv)
{
	xsock_test (1);
	xexp_test ();
	tcp_sg_send ();
	return 0;
}
