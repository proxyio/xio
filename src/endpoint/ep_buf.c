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

#include <stdio.h>
#include <stdarg.h>
#include "ep_struct.h"

static void ephdr_init(struct ephdr *h) {
    h->version = 0xff;
    h->ttl = 0;
    h->end_ttl = 0;
    h->go = 0;
    h->size = 0;
    h->timeout = 0;
    h->sendstamp = 0;
}

char *xep_allocubuf(int flags, int size, ...) {
    va_list ap;
    struct uhdr *uh;
    struct ephdr *h1 = 0, *h2 = 0;

    if (flags & XEPBUF_CLONEHDR) {
	va_start(ap, size);
	h1 = ubuf2ephdr(va_arg(ap, char *));
	va_end(ap);
    }
    size += h1 ? ephdr_ctlen(h1) :
	sizeof(struct ephdr) + sizeof(struct epr) + sizeof(struct uhdr);
    if (!(h2 = (struct ephdr *)xallocmsg(size)))
	return 0;
    if (!h1) {
	ephdr_init(h2);
	h2->ttl = 1;
	h2->go = 1;
	h2->size = size;
	uh = ephdr_uhdr(h2);
	uh->ephdr_off = ephdr_ctlen(h2);
    } else {
	memcpy((char *)h2, (char *)h1, ephdr_ctlen(h1));
    }
    return ephdr2ubuf(h2);
}

void xep_freeubuf(char *ubuf) {
    struct ephdr *h = ubuf2ephdr(ubuf);
    xfreemsg((char *)h);
}
