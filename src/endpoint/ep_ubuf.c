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

char *ubuf_clone(int flags, int size, char *ubuf) {
    struct uhdr *uh;
    struct ephdr *h1 = ubuf2ephdr(ubuf), *h2;
    int ctlen = ephdr_ctlen(h1);

    ctlen += (flags & XEPUBUF_APPENDRT) ? sizeof(struct epr) : 0;
    if (!(h2 = (struct ephdr *)xallocmsg(size + ctlen)))
	return 0;
    memcpy((char *)h2, (char *)h1, ephdr_ctlen(h1));
    if (flags & XEPUBUF_APPENDRT) {
	h2->ttl++;
	uh = ephdr_uhdr(h2);
	uh->ephdr_off += sizeof(struct epr);
	BUG_ON(uh->ephdr_off != ephdr_ctlen(h2));
    }
    return ephdr2ubuf(h2);
}

char *ubuf_new(int size) {
    int ctlen = sizeof(struct ephdr) + sizeof(struct epr) + sizeof(struct uhdr);
    struct uhdr *uh;
    struct ephdr *eh;

    if (!(eh = (struct ephdr *)xallocmsg(size + ctlen)))
	return 0;
    ephdr_init(eh);
    eh->ttl = 1;
    eh->go = 1;
    eh->size = size;
    uh = ephdr_uhdr(eh);
    uh->ephdr_off = ephdr_ctlen(eh);
    return ephdr2ubuf(eh);
}

char *xep_allocubuf(int flags, int size, ...) {
    va_list ap;
    char *ubuf;

    if (flags & XEPUBUF_CLONEHDR) {
	va_start(ap, size);
	ubuf = va_arg(ap, char *);
	va_end(ap);
	return ubuf_clone((flags & ~XEPUBUF_CLONEHDR), size, ubuf);
    }
    return ubuf_new(size);
}

void xep_freeubuf(char *ubuf) {
    struct ephdr *h = ubuf2ephdr(ubuf);
    xfreemsg((char *)h);
}

u32 xep_ubuflen(char *ubuf) {
    struct ephdr *h = ubuf2ephdr(ubuf);
    return h->size;
}
