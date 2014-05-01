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
#include <os/alloc.h>
#include <hash/crc.h>
#include "xmsg.h"


u32 xiov_len(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    return sizeof(msg->vec) + msg->vec.size;
}

char *xiov_base(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    return (char *)&msg->vec;
}

char *xallocmsg(int size) {
    char *xbuf;
    struct xmsg *msg;

    char *chunk = (char *)mem_zalloc(sizeof(*msg) + size);
    if (!chunk)
	return 0;
    msg = (struct xmsg *)chunk;
    msg->vec.size = size;
    msg->vec.checksum = crc16((char *)&msg->vec.size, 4);
    xbuf = msg->vec.chunk;
    DEBUG_OFF("%p", xbuf);
    return xbuf;
}

void xfreemsg(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);

    DEBUG_OFF("%p", xbuf);
    mem_free(msg, sizeof(*msg) + msg->vec.size);
}

int xmsglen(char *xbuf) {
    struct xmsg *msg = cont_of(xbuf, struct xmsg, vec.chunk);
    return msg->vec.size;
}

