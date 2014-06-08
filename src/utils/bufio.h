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

#ifndef _XIO_BIO_
#define _XIO_BIO_

#include <inttypes.h>
#include "base.h"
#include "list.h"

typedef struct bio_page {
	uint64_t start;
	uint64_t end;
	char page[PAGE_SIZE];
	struct list_head page_link;
} bio_page_t;


struct bio {
	uint64_t bsize;
	int64_t pno;
	struct list_head page_head;
};

struct bio *bio_new();
static inline void bio_init(struct bio *b)
{
	INIT_LIST_HEAD(&b->page_head);
}

static inline int64_t bio_size(struct bio *b)
{
	return b->bsize;
}

void bio_destroy(struct bio *b);
void bio_reset(struct bio *b);
int bio_empty(struct bio *b);
int64_t bio_copy(struct bio *b, char *buff, int64_t sz);
int64_t bio_read(struct bio *b, char *buff, int64_t sz);
int64_t bio_write(struct bio *b, const char *buff, int64_t sz);

int64_t bio_flush(struct bio *b, struct io *io_ops);
int64_t bio_prefetch(struct bio *b, struct io *io_ops);


#endif
