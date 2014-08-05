#include <errno.h>
#include <time.h>
#include <string.h>
#include <utils/spinlock.h>
#include <utils/thread.h>
#include <xio/poll.h>
#include <xio/socket.h>
#include <xio/cmsghdr.h>
#include <msgbuf/msgbuf.h>
#include <msgbuf/msgbuf_head.h>
#include "testutil.h"

static int isempty;
static void msgbuf_head_empty_ev_hndl (struct msgbuf_head *bh)
{
	isempty = 1;
}

static void msgbuf_head_nonempty_ev_hndl (struct msgbuf_head *bh)
{
	isempty = 0;
}


static int isfull;
static void msgbuf_head_full_ev_hndl (struct msgbuf_head *bh)
{
	isfull = 1;
}
static void msgbuf_head_nonfull_ev_hndl (struct msgbuf_head *bh)
{
	isfull = 0;
}

static int size;
static void msgbuf_head_add_ev_hndl (struct msgbuf_head *bh)
{
	size++;
}

static void msgbuf_head_rm_ev_hndl (struct msgbuf_head *bh)
{
	size--;
}


static struct msgbuf_vfptr msgbuf_vfptr = {
	.add = msgbuf_head_add_ev_hndl,
	.rm = msgbuf_head_rm_ev_hndl,
	.empty = msgbuf_head_empty_ev_hndl,
	.nonempty = msgbuf_head_nonempty_ev_hndl,
	.full = msgbuf_head_full_ev_hndl,
	.nonfull = msgbuf_head_nonfull_ev_hndl,
};


int main (int argc, char **argv)
{
	struct msgbuf_head bh;
	char *ubuf;

	msgbuf_head_init (&bh, 1000);
	msgbuf_head_ev_hndl (&bh, &msgbuf_vfptr);
	assert (isempty == 0 && size == 0 && isfull == 0);

	msgbuf_head_in (&bh, ubuf_alloc (500));
	assert (isempty == 0 && size == 1 && isfull == 0);

	msgbuf_head_in (&bh, ubuf_alloc (500));
	assert (isempty == 0 && size == 2 && isfull == 1);

	msgbuf_head_in (&bh, ubuf_alloc (500));
	assert (isempty == 0 && size == 3 && isfull == 1);

	msgbuf_head_out (&bh, &ubuf);
	ubuf_free (ubuf);
	assert (isempty == 0 && size == 2 && isfull == 1);

	msgbuf_head_out (&bh, &ubuf);
	ubuf_free (ubuf);
	assert (isempty == 0 && size == 1 && isfull == 0);

	msgbuf_head_out (&bh, &ubuf);
	ubuf_free (ubuf);
	assert (isempty == 1 && size == 0 && isfull == 0);

	msgbuf_head_in (&bh, ubuf_alloc (1000));
	assert (isempty == 0 && size == 1 && isfull == 1);

	msgbuf_head_out (&bh, &ubuf);
	ubuf_free (ubuf);
	assert (isempty == 1 && size == 0 && isfull == 0);

	msgbuf_head_in (&bh, ubuf_alloc (1100));
	assert (isempty == 0 && size == 1 && isfull == 1);

	msgbuf_head_out (&bh, &ubuf);
	ubuf_free (ubuf);
	assert (isempty == 1 && size == 0 && isfull == 0);
	
	return 0;
}

