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

struct epr *epr_new() {
    struct epr *r = (struct epr *)xallocmsg(sizeof(struct epr));
    BUG_ON(!r);
    return r;
}

void epr_free(struct epr *r) {
    xfreemsg((char *)r);
}

struct ephdr *ephdr_new() {
    struct ephdr *eh = (struct ephdr *)xallocmsg(sizeof(struct ephdr));
    if (eh) {
	eh->version = 0xff;
	eh->ttl = 0;
	eh->end_ttl = 0;
	eh->go = 0;
	eh->size = 0;
	eh->timeout = 0;
	eh->sendstamp = 0;
    }
    return eh;
}

void ephdr_free(struct ephdr *eh) {
    xfreemsg((char *)eh);
}

char *xep_allocubuf(int size) {
    int cmsgnum;
    struct xcmsg oob;
    char *ubuf = xallocmsg(size);

    BUG_ON(!ubuf);
    oob.idx = 0;
    oob.outofband = (char *)ephdr_new();
    BUG_ON(xmsgctl(ubuf, XMSG_SETCMSG, &oob) != 0);
    BUG_ON(xmsgctl(ubuf, XMSG_CMSGNUM, &cmsgnum) != 0);
    BUG_ON(cmsgnum != 1);
    return ubuf;
}

void xep_freeubuf(char *ubuf) {
    xfreemsg((char *)ubuf);
}

u32 xep_ubuflen(char *ubuf) {
    return xmsglen(ubuf);
}
