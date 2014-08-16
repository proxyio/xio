#include <errno.h>
#include <time.h>
#include <string.h>
#include <xio/sp_reqrep.h>
#include <xio/socket.h>
#include <utils/timer.h>
#include <utils/spinlock.h>
#include <utils/thread.h>

static int argc;
static char **argv;

int task_main (void *args)
{
	int i;
	int j;
	int conns = 40;
	int eid;
	int size;
	int rts;
	char *ubuf;
	int64_t st, lt;

	if (argc < 4) {
		printf ("usage: lat_sender <connect-to> <msg-size> <roundtrips>\n");
		return 0;
	}

	size = atoi (argv[2]);
	rts = atoi (argv[3]);
	BUG_ON ((eid = sp_endpoint (SP_REQREP, SP_REQ)) < 0);
	for (i = 0; i < conns; i++)
		BUG_ON (sp_connect (eid, argv[1]) < 0);

	st = gettimeofus ();
	for (i = 0; i < rts; i++) {
		for (j = 0; j < conns; j++)
			BUG_ON (sp_send (eid, ubuf_alloc (size)) != 0);
		for (j = 0; j < conns; j++) {
			BUG_ON (sp_recv (eid, &ubuf) != 0);
			ubuf_free (ubuf);
		}
	}
	lt = gettimeofus ();

	printf ("message size: %d [B]\n", size);
	printf ("roundtrip count: %d\n", rts * conns);
	printf ("average latency: %.3f [us]\n", (double)(lt - st) / (rts * conns * 2));
	sp_close (eid);
	return 0;
}

int main (int _argc, char **_argv)
{
	int i;
	thread_t childs[1];

	argc = _argc;
	argv = _argv;
	
	for (i = 0; i < NELEM (childs, thread_t); i++)
		thread_start (&childs[i], task_main, 0);
	for (i = 0; i < NELEM (childs, thread_t); i++)
		thread_stop (&childs[i]);
	return 0;
}
