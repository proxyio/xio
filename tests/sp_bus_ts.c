#include <errno.h>
#include <time.h>
#include <string.h>
#include <xio/sp_bus.h>
#include <utils/thread.h>
#include "testutil.h"

static char *hello = "hello world";

int main (int argc, char **argv)
{
	int n_eid1 = sp_endpoint (SP_BUS, SP_BUS);
	int n_eid2 = sp_endpoint (SP_BUS, SP_BUS);
	int cli1 = sp_endpoint (SP_BUS, SP_BUS);
	int cli2 = sp_endpoint (SP_BUS, SP_BUS);
	int rc;
	char *ubuf;

	rc = sp_listen (n_eid1, "tcp://127.0.0.1:15100");
	BUG_ON (rc < 0);

	rc = sp_listen (n_eid2, "tcp://127.0.0.1:15200");
	BUG_ON (rc < 0);

	rc = sp_connect (n_eid1, "tcp://127.0.0.1:15200");
	BUG_ON (rc < 0);

	rc = sp_connect (cli1, "tcp://127.0.0.1:15100");
	BUG_ON (rc < 0);

	rc = sp_connect (cli2, "tcp://127.0.0.1:15200");
	BUG_ON (rc < 0);

	ubuf = ubuf_alloc (strlen (hello));
	memcpy (ubuf, hello, strlen (hello));

	rc = sp_send (cli1, ubuf);
	BUG_ON (rc);

	rc = sp_recv (cli2, &ubuf);
	BUG_ON (rc);

	BUG_ON (memcmp (ubuf, hello, strlen (hello)) != 0);
	ubuf_free (ubuf);
	
	sp_close (n_eid1);
	sp_close (n_eid2);
	sp_close (cli1);	
	sp_close (cli2);
}
