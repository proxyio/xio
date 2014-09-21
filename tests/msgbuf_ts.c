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


static void test_ev_hndl ()
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
}

static char *simple_mk_ubuf (const char *str)
{
	char *ubuf = ubuf_alloc (strlen (str) + 1);
	strcpy (ubuf, str);
	return ubuf;
}

/*
 *    (hello)
 *    /     \
 *  (ok)   (yes)
 *         /   \
 *   (apple)   (acer)
 *   /    \      \
 * (s4)  (s5)    (c50)
 *
 * after install into iovs:
 * (hello) (ok) (yes) (apple) (s4) (s5) (acer) (c50)
 */

static void test_install_iovs ()
{
	struct rex_iov iov[100];
	char *hello = simple_mk_ubuf ("hello");
	char *ok = simple_mk_ubuf ("ok");
	char *yes = simple_mk_ubuf ("yes");
	char *apple = simple_mk_ubuf ("apple");
	char *s4 = simple_mk_ubuf ("s4");
	char *s5 = simple_mk_ubuf ("s5");
	char *acer = simple_mk_ubuf ("acer");
	char *c50 = simple_mk_ubuf ("c50");
	char *hello2 = simple_mk_ubuf ("hello2");
	u32 length = 113;
	u32 tmp_length[4];
	int install[4];
	struct msgbuf_head bh;
	
	msgbuf_head_init (&bh, 1);
	msgbuf_head_in (&bh, hello);
	msgbuf_head_in (&bh, hello2);

	ubufctl (apple, SADD, s4);
	ubufctl (apple, SADD, s5);
	ubufctl (acer, SADD, c50);
	ubufctl (yes, SADD, apple);
	ubufctl (yes, SADD, acer);
	ubufctl (hello, SADD, ok);
	ubufctl (hello, SADD, yes);

	BUG_ON (msgbuf_preinstall_iovs (get_msgbuf (hello), iov, 1) != 1);
	BUG_ON (msgbuf_preinstall_iovs (get_msgbuf (hello), iov, 2) != 2);
	BUG_ON (msgbuf_preinstall_iovs (get_msgbuf (hello), iov, 20) != 8);

	tmp_length[0] = rand () % 28;
	tmp_length[1] = rand () % 28;
	tmp_length[2] = rand () % 28;
	tmp_length[3] = length - tmp_length[0] - tmp_length[1] - tmp_length[2];

	
	install[0] = msgbuf_head_install_iovs (&bh, iov, tmp_length[0]);
	install[1] = msgbuf_head_install_iovs (&bh, iov, tmp_length[1]);
	install[2] = msgbuf_head_install_iovs (&bh, iov, tmp_length[2]);
	install[3] = msgbuf_head_install_iovs (&bh, iov, tmp_length[3]);

	BUG_ON (install[0] + install[1] + install[2] + install[3] != 2);
	ubuf_free (hello);
}


int main (int argc, char **argv)
{
	int i;
	test_ev_hndl ();

	for (i = 0; i < 1000; i++)
		test_install_iovs ();
	return 0;
}
