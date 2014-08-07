#include <errno.h>
#include <time.h>
#include <string.h>
#include <xio/sp_reqrep.h>
#include <xio/socket.h>
#include <utils/timer.h>
#include <utils/spinlock.h>
#include <utils/thread.h>

int main (int argc, char **argv)
{
	int s;
	int i;
	int eid;
	int size;
	int count = 0x7fffffff;
	char *ubuf;
	int qps;
	int64_t st, lt;

	if (argc < 3) {
		printf ("lat_sender tcp:127.0.0.1:1880 100 100000\n");
		return 0;
	}

	size = atoi (argv[2]);
	if (argc > 3)
		count = atoi (argv[3]);
	BUG_ON ((eid = sp_endpoint (SP_REQREP, SP_REQ)) < 0);
	for (i = 0; i < 8; i++)
		BUG_ON (sp_connect (eid, argv[1]) < 0);
	st = gettimeofms ();
	for (i = 0; i < count; i++, qps++) {
		BUG_ON (sp_send (eid, ubuf_alloc (size)) != 0);
		BUG_ON (sp_recv (eid, &ubuf) != 0);
		ubuf_free (ubuf);
		if (count % 1000 == 0 && (lt = gettimeofms ()) - st > 1000) {
			st = lt;
			printf ("qps %d/s\n", qps);
			qps = 0;
		}
	}
	sp_close (eid);
	return 0;
}
