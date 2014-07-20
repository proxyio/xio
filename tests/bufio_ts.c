#include <utils/bufio.h>
#include "testutil.h"

char buf1[PAGE_SIZE * 4] = {}, buf2[PAGE_SIZE * 4] = {};

static void bufio_test()
{
	struct bio b = {};
	int i;
	int64_t rlen = rand() % PAGE_SIZE + 3 * PAGE_SIZE;

	bio_init (&b);
	randstr (buf1, sizeof (buf1));
	for (i = 0; i < rlen; i++)
		BUG_ON (1 != bio_write (&b, buf1 + i, 1));
	BUG_ON (b.bsize != rlen);
	BUG_ON (rlen != bio_copy (&b, buf2, 4 * PAGE_SIZE));
	BUG_ON (rlen != bio_read (&b, buf2, 4 * PAGE_SIZE));
	BUG_ON (b.bsize != 0);
	BUG_ON (!list_empty (&b.page_head));
	BUG_ON (!memcmp (buf1, buf2, rlen) == 0);
	bio_destroy (&b);
}

struct test_io {
	int64_t ridx;
	int64_t widx;
	struct io io_ops;
};

static int64_t test_io_read (struct io *ops, char *buff, int64_t sz)
{
	struct test_io *tio = cont_of (ops, struct test_io, io_ops);
	int64_t len = rand() % PAGE_SIZE;

	/* Don't overload buf1 here */
	if (rand() % 2 == 0 || tio->ridx >= 4 * PAGE_SIZE) {
		errno = EAGAIN;
		return -1;
	}
	len = len > sz ? sz : len;
	if (len + tio->ridx >= 4 * PAGE_SIZE)
		len = 4 * PAGE_SIZE - tio->ridx;
	BUG_ON (tio->ridx > 4 * PAGE_SIZE);
	BUG_ON (len < 0);
	memcpy (buff, buf1 + tio->ridx, len);
	tio->ridx += len;
	return len;
}

static int64_t test_io_write (struct io *ops, char *buff, int64_t sz)
{
	struct test_io *tio = cont_of (ops, struct test_io, io_ops);
	int64_t len = rand() % PAGE_SIZE;

	/* Don't overload buf2 here */
	if (rand() % 2 == 0 || tio->widx >= 4 * PAGE_SIZE) {
		errno = EAGAIN;
		return -1;
	}
	len = len > sz ? sz : len;
	if (len + tio->widx >= 4 * PAGE_SIZE)
		len = 4 * PAGE_SIZE - tio->widx;
	BUG_ON (tio->widx > 4 * PAGE_SIZE);
	BUG_ON (len < 0);
	memcpy (buf2 + tio->widx, buff, len);
	tio->widx += len;
	return len;
}

static void bufio_fetch_flush_test()
{
	struct bio b = {};
	int i;
	struct test_io tio = {};
	uint64_t nbytes = 0;
	int64_t rc;

	bio_init (&b);
	tio.io_ops.read = test_io_read;
	tio.io_ops.write = test_io_write;
	randstr (buf1, sizeof (buf1));
	for (i = 0; i < 4; i++) {
		if ((rc =  bio_prefetch (&b, &tio.io_ops)) > 0)
			nbytes += rc;
	}
	BUG_ON (tio.ridx > 4 * PAGE_SIZE);
	BUG_ON (nbytes != tio.ridx);
	BUG_ON (nbytes > 4 * PAGE_SIZE);
	bio_copy (&b, buf2, nbytes);
	BUG_ON (memcmp (buf1, buf2, nbytes) != 0);
	bio_copy (&b, buf2, nbytes);
	BUG_ON (memcmp (buf1, buf2, nbytes) != 0);
	BUG_ON (b.bsize != nbytes);
	while (nbytes > 0) {
		if ((rc =  bio_flush (&b, &tio.io_ops)) > 0)
			nbytes -= rc;
	}
	BUG_ON (tio.widx > 4 * PAGE_SIZE);
	BUG_ON (b.bsize != 0);
	BUG_ON (!list_empty (&b.page_head));
	BUG_ON (memcmp (buf1, buf2, nbytes) != 0);
	bio_destroy (&b);
}

int main (int argc, char **argv)
{
	bufio_test();
	bufio_fetch_flush_test();
	return 0;
}
