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
	int s;
	int i, j;
	int eid;
	int size;
	int count;
	char *ubuf;
	int conn;
	int64_t st, lt;

	if (argc < 4) {
		printf ("lat_sender tcp:127.0.0.1:1880 100 100000\n");
		return 0;
	}

	size = atoi (argv[2]);
	count = atoi (argv[3]);
	BUG_ON ((eid = sp_endpoint (SP_REQREP, SP_REQ)) < 0);
	BUG_ON (sp_connect (eid, argv[1]) < 0);
	st = gettimeofms ();
	for (i = 0; i < count; i++)
		BUG_ON (sp_send (eid, ubuf_alloc (size)) != 0);
	lt = gettimeofms ();
	printf ("%d qps %"PRId64"/s\n", getpid (), count * 1000 / (lt - st));
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
