#include <errno.h>
#include <time.h>
#include <string.h>
#include <xio/sp_reqrep.h>
#include <xio/socket.h>
#include <utils/spinlock.h>
#include <utils/thread.h>
#include "testutil.h"

static int req_thread (void *args)
{
	char host[1024];
	char buf[128];
	int s;
	int i;
	int eid;
	char *sbuf, *rbuf;

	sprintf (host, "%s%s", (char *) args, "://127.0.0.1:15100");
	randstr (buf, sizeof (buf) );
	BUG_ON ( (eid = sp_endpoint (SP_REQREP, SP_REQ) ) < 0);
	for (i = 0; i < 1; i++) {
		BUG_ON ( (s = xconnect (host) ) < 0);
		BUG_ON (sp_add (eid, s) < 0);
	}
	for (i = 0; i < 9; i++) {
		sbuf = rbuf = 0;
		sbuf = xallocubuf (sizeof (buf) );
		memcpy (sbuf, buf, sizeof (buf) );
		DEBUG_OFF ("producer %d send %d request: %10.10s", eid, i, sbuf);
		BUG_ON (sp_send (eid, sbuf) != 0);
		while (sp_recv (eid, &rbuf) != 0) {
			usleep (10000);
		}
		DEBUG_OFF ("producer %d recv %d resp: %10.10s", eid, i, rbuf);
		DEBUG_OFF ("----------------------------------------");
		BUG_ON (xubuflen (rbuf) != sizeof (buf) );
		BUG_ON (memcmp (rbuf, buf, sizeof (buf) ) != 0);
		xfreeubuf (rbuf);
	}
	DEBUG_OFF ("producer %d close on %s", eid, host);
	sp_close (eid);
	return 0;
}


int server1()
{
	char *addr = "://127.0.0.1:15100";
	char host[1024] = {};
	u32 i;
	thread_t t[30];
	const char *pf[] = {
		"ipc",
		"tcp",
		"inproc",
	};
	int s;
	int eid;
	char *ubuf;

	BUG_ON ( (eid = sp_endpoint (SP_REQREP, SP_REP) ) < 0);
	for (i = 0; i < NELEM (pf, const char *); i++) {
		sprintf (host, "%s%s", pf[i], addr);
		BUG_ON ( (s = sp_listen (eid, host) ) < 0);
	}
	for (i = 0; i < NELEM (t, thread_t); i++) {
		thread_start (&t[i], req_thread, (void *) pf[rand() % 3]);
	}
	sleep (1);
	for (i = 0; i < NELEM (t, thread_t) * 9; i++) {
		while (sp_recv (eid, &ubuf) != 0) {
			usleep (10000);
		}
		DEBUG_OFF ("comsumer %d recv %d requst: %10.10s", eid, i, ubuf);
		BUG_ON (sp_send (eid, ubuf) );
	}
	for (i = 0; i < NELEM (t, thread_t); i++) {
		thread_stop (&t[i]);
	}
	sp_close (eid);
	return 0;
}






static int rep_thread (void *args)
{
	char host[1024], *ubuf;
	int i;
	int eid;

	sprintf (host, "%s%s", (char *) args, "://127.0.0.1:15200");
	BUG_ON ( (eid = sp_endpoint (SP_REQREP, SP_REP) ) < 0);
	BUG_ON (sp_listen (eid, host) < 0);

	for (i = 0; i < 30; i++) {
		while (sp_recv (eid, &ubuf) != 0) {
			usleep (10000);
		}
		DEBUG_OFF ("comsumer %d recv %d requst: %10.10s", eid, i, ubuf);
		BUG_ON (sp_send (eid, ubuf) );
	}
	DEBUG_OFF ("comsumer %d close on %s", eid, host);
	sp_close (eid);
	return 0;
}

int server2()
{
	char *addr = "://127.0.0.1:15200";
	char host[1024] = {};
	char buf[128];
	u32 i;
	thread_t t[3];
	const char *pf[] = {
		"ipc",
		"tcp",
		"inproc",
	};
	int s;
	int eid;
	char *sbuf, *rbuf;

	for (i = 0; i < NELEM (t, thread_t); i++) {
		thread_start (&t[i], rep_thread, (void *) pf[i]);
	}
	sleep (1);

	BUG_ON ( (eid = sp_endpoint (SP_REQREP, SP_REQ) ) < 0);
	sbuf = xallocubuf (0);
	BUG_ON (sp_send (eid, sbuf) != -1);

	for (i = 0; i < NELEM (t, thread_t); i++) {
		sprintf (host, "%s%s", pf[i], addr);
		BUG_ON ( (s = sp_connect (eid, host) ) < 0);
	}
	randstr (buf, sizeof (buf) );
	for (i = 0; i < NELEM (t, thread_t) * 30; i++) {
		sbuf = rbuf = 0;
		sbuf = xallocubuf (sizeof (buf) );
		memcpy (sbuf, buf, sizeof (buf) );
		DEBUG_OFF ("producer %d send %d request: %10.10s", eid, i, sbuf);
		BUG_ON (sp_send (eid, sbuf) != 0);
		while (sp_recv (eid, &rbuf) != 0) {
			usleep (10000);
		}
		DEBUG_OFF ("producer %d recv %d resp: %10.10s", eid, i, rbuf);
		DEBUG_OFF ("----------------------------------------");
		BUG_ON (xubuflen (rbuf) != sizeof (buf) );
		BUG_ON (memcmp (rbuf, buf, sizeof (buf) ) != 0);
		xfreeubuf (rbuf);
	}
	for (i = 0; i < NELEM (t, thread_t); i++) {
		thread_stop (&t[i]);
	}
	sp_close (eid);
	return 0;
}


int main (int argc, char **argv)
{
	server1();
	server2();
}
