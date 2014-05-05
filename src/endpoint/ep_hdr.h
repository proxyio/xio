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

#ifndef _XIO_EPHDR_
#define _XIO_EPHDR_

#include <errno.h>
#include <uuid/uuid.h>
#include <ds/list.h>
#include <os/timesz.h>
#include <hash/crc.h>

/* The ephdr looks like this:
 * +-------+---------+-------+--------+
 * | ephdr |  epr[]  |  uhdr |  ubuf  |
 * +-------+---------+-------+--------+
 */

#define XEPUBUF_APPENDRT 0x02

struct epr {
    uuid_t uuid;
    u8 ip[4];
    u16 port;
    u16 begin[2];
    u16 cost[2];
    u16 stay[2];
};

struct epr *epr_new();
void epr_free(struct epr *r);

struct ephdr {
    u8 version;
    u16 ttl:4;
    u16 end_ttl:4;
    u16 go:1;
    u32 size;
    u16 timeout;
    i64 sendstamp;
    union {
	u64 __align[2];
	struct list_head link;
    } u;
    struct epr rt[0];
};

struct ephdr *ephdr_new();
void ephdr_free(struct ephdr *eh);

static inline struct ephdr *ubuf2ephdr(char *ubuf) {
    int cmsgnum = 0;
    struct xcmsg ent;

    BUG_ON(xmsgctl(ubuf, XMSG_CMSGNUM, &cmsgnum) != 0);
    BUG_ON(!cmsgnum);
    ent.idx = 0;
    BUG_ON(xmsgctl(ubuf, XMSG_GETCMSG, &ent) != 0);
    return (struct ephdr *)ent.outofband;
}

static inline int ephdr_timeout(struct ephdr *h) {
    return h->timeout && (h->sendstamp + h->timeout < rt_mstime());
}

static inline struct epr *rt_cur(char *ubuf) {
    struct ephdr *eh = ubuf2ephdr(ubuf);
    int cmsgnum = 0;
    struct xcmsg ent;

    BUG_ON(xmsgctl(ubuf, XMSG_CMSGNUM, &cmsgnum) != 0);
    BUG_ON(!cmsgnum && cmsgnum != eh->ttl + 1);
    ent.idx = eh->ttl;
    BUG_ON(xmsgctl(ubuf, XMSG_GETCMSG, &ent) != 0);
    return (struct epr *)ent.outofband;
}

static inline struct epr *rt_prev(char *ubuf) {
    struct ephdr *eh = ubuf2ephdr(ubuf);
    int cmsgnum = 0;
    struct xcmsg ent;

    BUG_ON(xmsgctl(ubuf, XMSG_CMSGNUM, &cmsgnum) != 0);
    BUG_ON(!cmsgnum && cmsgnum != eh->ttl + 1);
    ent.idx = eh->ttl - 1;
    BUG_ON(xmsgctl(ubuf, XMSG_GETCMSG, &ent) != 0);
    return (struct epr *)ent.outofband;
}

static inline void rt_append(char *ubuf, struct epr *r) {
    struct ephdr *eh = ubuf2ephdr(ubuf);
    int cmsgnum = 0;
    struct xcmsg ent;
    
    BUG_ON(xmsgctl(ubuf, XMSG_CMSGNUM, &cmsgnum) != 0);
    BUG_ON(!cmsgnum && cmsgnum != eh->ttl + 1);
    ent.idx = ++eh->ttl;
    ent.outofband = (char *)r;
    BUG_ON(xmsgctl(ubuf, XMSG_SETCMSG, &ent) != 0);
}



#endif
