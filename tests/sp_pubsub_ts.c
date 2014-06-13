#include <errno.h>
#include <time.h>
#include <string.h>
#include <xio/sp_pubsub.h>
#include <xio/socket.h>
#include <xio/cmsghdr.h>
#include <utils/thread.h>
#include "testutil.h"

static char *hello = "hello world";

static int sub_thread (void *args)
{
	char host[1024];
	char buf[128];
	int rc;
	int i;
	int eid;
	char *ubuf;

	sprintf (host, "%s%s", (char *) args, "://127.0.0.1:15100");
	randstr (buf, sizeof (buf) );
	BUG_ON ( (eid = sp_endpoint (SP_PUBSUB, SP_SUB) ) < 0);
	BUG_ON ((rc = sp_connect (eid, host)) < 0);

	for (i = 0; i < 5; i++) {
		while (sp_recv (eid, &ubuf) != 0) {
			usleep (10000);
		}
		DEBUG_OFF ("%d sub %d %10.10s", eid, i, ubuf);
		BUG_ON (memcmp (ubuf, hello, strlen(hello)) != 0);
		xfreeubuf (ubuf);
	}
	sp_close (eid);
	return 0;
}


int server1()
{
	char *addr = "://127.0.0.1:15100";
	char host[1024] = {};
	u32 i;
	thread_t t[20];
	const char *pf[] = {
		"ipc",
		"tcp",
		"inproc",
	};
	int s;
	int eid;
	char *ubuf;

	BUG_ON ( (eid = sp_endpoint (SP_PUBSUB, SP_PUB) ) < 0);
	for (i = 0; i < NELEM (pf, const char *); i++) {
		sprintf (host, "%s%s", pf[i], addr);
		BUG_ON ( (s = sp_listen (eid, host) ) < 0);
	}
	for (i = 0; i < NELEM (t, thread_t); i++) {
		thread_start (&t[i], sub_thread, (void *) pf[rand() % 3]);
	}
	sleep (1);
	for (i = 0; i < 5; i++) {
		ubuf = xallocubuf (strlen(hello));
		memcpy (ubuf, hello, strlen(hello));
		DEBUG_OFF ("%d pub %d %10.10s", eid, i, ubuf);
		BUG_ON (sp_send (eid, ubuf) );
	}
	for (i = 0; i < NELEM (t, thread_t); i++) {
		thread_stop (&t[i]);
	}
	sp_close (eid);
	return 0;
}

int main (int argc, char **argv)
{
	char *ubuf = xallocubuf (1024);
	char *dst;

	ubufctl (ubuf, SCLONE, &dst);
	xfreeubuf (ubuf);
	xfreeubuf (dst);

	server1();
}
