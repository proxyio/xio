#include <errno.h>
#include <time.h>
#include <string.h>
#include <xio/sp_reqrep.h>
#include <xio/socket.h>
#include <utils/spinlock.h>
#include <utils/thread.h>

int main (int argc, char **argv)
{
	int i;
	int eid;
	int count = 0x7fffffff;
	char *ubuf;

	if (argc < 2) {
		printf ("lat_recver tcp:127.0.0.1:1880 100000\n");
		return 0;
	}
	if (argc > 2)
		count = atoi (argv[2]);
	BUG_ON ((eid = sp_endpoint (SP_REQREP, SP_REP)) < 0);
	BUG_ON (sp_listen (eid, argv[1]) < 0);

	for (i = 0; i < count; i++) {
		BUG_ON (sp_recv (eid, &ubuf) != 0);
		BUG_ON (sp_send (eid, ubuf));
	}
	sp_close (eid);
	return 0;
}
