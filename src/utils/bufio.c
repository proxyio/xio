/*
  Copyright (c) 2013-2014 Dong Fang. All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#include <errno.h>
#include "bufio.h"
#include "alloc.h"

#define walk_each_page_s(bp, tmp, head)				\
    walk_each_entry_s(bp, tmp, head, bio_page_t, page_link)

int bio_empty (struct bio *b)
{
	if (b->bsize == 0)
		return true;
	return false;
}

static inline bio_page_t *bio_first (struct bio *b)
{
	bio_page_t *bp = NULL;
	if (!list_empty (&b->page_head))
		bp = list_first (&b->page_head, bio_page_t, page_link);
	return bp;
}

static inline bio_page_t *bio_last (struct bio *b)
{
	bio_page_t *bp = NULL;
	if (!list_empty (&b->page_head))
		bp = list_last (&b->page_head, bio_page_t, page_link);
	return bp;
}


static inline bio_page_t *bio_page_new()
{
	bio_page_t *bp = mem_zalloc (sizeof (bio_page_t));
	return bp;
}

static inline int bio_page_empty (bio_page_t *bp)
{
	if (bp->end == bp->start)
		return true;
	return false;
}

static inline int bio_page_full (bio_page_t *bp)
{
	if (bp->end - bp->start == PAGE_SIZE)
		return true;
	return false;
}

static inline int64_t
bio_page_copy (bio_page_t *bp, char *buff, int64_t sz)
{
	sz = (bp->end - bp->start) > sz ? sz : bp->end - bp->start;
	memcpy (buff, bp->page + bp->start, sz);
	return sz;
}

static inline int64_t
bio_page_read (bio_page_t *bp, char *buff, int64_t sz)
{
	sz = (bp->end - bp->start) > sz ? sz : bp->end - bp->start;
	memcpy (buff, bp->page + bp->start, sz);
	bp->start += sz;
	memmove (bp->page, bp->page + bp->start, bp->end - bp->start);
	bp->end = bp->end - bp->start;
	bp->start = 0;
	return sz;
}

static inline int64_t
bio_page_write (bio_page_t *bp, const char *buff, int64_t sz)
{
	sz = (PAGE_SIZE - (bp->end - bp->start)) > sz ?
	     sz : (PAGE_SIZE - (bp->end - bp->start));
	memcpy (bp->page + bp->end, buff, sz);
	bp->end += sz;
	return sz;
}

struct bio *bio_new() {
	struct bio *b = mem_zalloc (sizeof (struct bio));
	return b;
}

void bio_reset (struct bio *b)
{
	bio_page_t *bp, *tmp;
	walk_each_page_s (bp, tmp, &b->page_head) {
		list_del (&bp->page_link);
		mem_free (bp, sizeof (*bp));
	}
}

void bio_destroy (struct bio *b)
{
	bio_reset (b);
}

static inline int bio_expand_one_page (struct bio *b)
{
	bio_page_t *bp = bio_page_new();
	if (!bp)
		return -1;
	b->pno++;
	list_add_tail (&bp->page_link, &b->page_head);
	return 0;
}

static inline int bio_shrink_one_page (struct bio *b)
{
	bio_page_t *bp;

	if (list_empty (&b->page_head))
		return -1;
	bp = bio_last (b);
	list_del (&bp->page_link);
	b->pno--;
	mem_free (bp, sizeof (*bp));
	return 0;
}

int64_t bio_copy (struct bio *b, char *buff, int64_t sz)
{
	bio_page_t *bp, *tmp;
	int64_t nbytes = 0;

	walk_each_page_s (bp, tmp, &b->page_head) {
		nbytes += bio_page_copy (bp, buff + nbytes, sz - nbytes);
		if (sz == nbytes)
			break;
	}
	return nbytes;
}

int64_t bio_read (struct bio *b, char *buff, int64_t sz)
{
	bio_page_t *bp;
	int64_t nbytes = 0;

	while (!list_empty (&b->page_head) && nbytes < sz) {
		bp = bio_first (b);
		nbytes += bio_page_read (bp, buff + nbytes, sz - nbytes);
		if (bio_page_empty (bp)) {
			list_del (&bp->page_link);
			mem_free (bp, sizeof (*bp));
		}
	}
	b->bsize -= nbytes;
	return nbytes;
}

static inline int is_time_to_expand (struct bio *b)
{
	bio_page_t *bp = bio_last (b);
	if (list_empty (&b->page_head) || bio_page_full (bp))
		return true;
	return false;
}


int64_t bio_write (struct bio *b, const char *buff, int64_t sz)
{
	bio_page_t *bp;
	int64_t nbytes = 0;

	while (nbytes < sz) {
		if (is_time_to_expand (b) && bio_expand_one_page (b) < 0)
			break;
		bp = bio_last (b);
		nbytes += bio_page_write (bp, buff + nbytes, sz - nbytes);
	}
	b->bsize += nbytes;
	return nbytes;
}


int64_t bio_flush (struct bio *b, struct io *io_ops)
{
	int64_t nbytes = 0, sum = 0;
	char page[PAGE_SIZE];

	while (!list_empty (&b->page_head)) {
		if ((nbytes = io_ops->write (io_ops, page, bio_copy (b, page,
		                              PAGE_SIZE))) < 0)
			break;
		sum += nbytes;
		BUG_ON (nbytes != bio_read (b, page, nbytes));
	}
	if (nbytes < 0 && errno != EAGAIN && sum == 0)
		return -1;
	return sum;
}

int64_t bio_prefetch (struct bio *b, struct io *io_ops)
{
	int64_t nbytes, sum = 0;
	char page[PAGE_SIZE];

	while ((nbytes = io_ops->read (io_ops, page, PAGE_SIZE)) >= 0)
		sum += bio_write (b, page, nbytes);
	if (nbytes < 0 && errno != EAGAIN && sum == 0)
		return -1;
	return sum;
}

