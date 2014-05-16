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

#ifndef _XIO_SPHDR_
#define _XIO_SPHDR_

#include <errno.h>
#include <uuid/uuid.h>
#include <ds/list.h>
#include <os/timesz.h>
#include <hash/crc.h>
#include <xio/socket.h>


/* The sphdr looks like this:
 * +-------+---------+-------+--------+
 * | sphdr |  spr[]  |  uhdr |  ubuf  |
 * +-------+---------+-------+--------+
 */

#define XSPUBUF_APPENDRT 0x02

struct spr {
    uuid_t uuid;
    u8 ip[4];
    u16 port;
    u16 begin[2];
    u16 cost[2];
    u16 stay[2];
};

struct spr *spr_new();
void spr_free(struct spr *r);

struct sphdr {
    u8 protocol;
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
};

struct sphdr *sphdr_new(u8 protocol, u8 version);
void sphdr_free(struct sphdr *eh);

static inline struct sphdr *ubuf2sphdr(char *ubuf) {
    int cmsgnum = 0;
    struct xcmsg ent;

    BUG_ON(xmsgctl(ubuf, XMSG_CMSGNUM, &cmsgnum) != 0);
    BUG_ON(!cmsgnum);
    ent.idx = 0;
    BUG_ON(xmsgctl(ubuf, XMSG_GETCMSG, &ent) != 0);
    return (struct sphdr *)ent.outofband;
}

static inline int sphdr_timeout(struct sphdr *h) {
    return h->timeout && (h->sendstamp + h->timeout < rt_mstime());
}

static inline struct spr *rt_cur(char *ubuf) {
    struct sphdr *eh = ubuf2sphdr(ubuf);
    int cmsgnum = 0;
    struct xcmsg ent;

    BUG_ON(xmsgctl(ubuf, XMSG_CMSGNUM, &cmsgnum) != 0);
    BUG_ON(!cmsgnum && cmsgnum != eh->ttl + 1);
    ent.idx = eh->ttl;
    BUG_ON(xmsgctl(ubuf, XMSG_GETCMSG, &ent) != 0);
    return (struct spr *)ent.outofband;
}

static inline struct spr *rt_prev(char *ubuf) {
    struct sphdr *eh = ubuf2sphdr(ubuf);
    int cmsgnum = 0;
    struct xcmsg ent;

    BUG_ON(xmsgctl(ubuf, XMSG_CMSGNUM, &cmsgnum) != 0);
    BUG_ON(!cmsgnum && cmsgnum != eh->ttl + 1);
    ent.idx = eh->ttl - 1;
    BUG_ON(xmsgctl(ubuf, XMSG_GETCMSG, &ent) != 0);
    return (struct spr *)ent.outofband;
}

static inline void rt_append(char *ubuf, struct spr *r) {
    struct sphdr *eh = ubuf2sphdr(ubuf);
    int cmsgnum = 0;
    struct xcmsg ent;
    
    BUG_ON(xmsgctl(ubuf, XMSG_CMSGNUM, &cmsgnum) != 0);
    BUG_ON(!cmsgnum && cmsgnum != eh->ttl + 1);
    ent.idx = ++eh->ttl;
    ent.outofband = (char *)r;
    BUG_ON(xmsgctl(ubuf, XMSG_SETCMSG, &ent) != 0);
}



#endif
