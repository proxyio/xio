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

#ifndef _XIO_RRHDR_
#define _XIO_RRHDR_

#include <sp/sp_hdr.h>

/* The sphdr looks like this:
 * +-------+---------+-------+--------+
 * | sphdr |  rrr[]  |  uhdr |  ubuf  |
 * +-------+---------+-------+--------+
 */

struct rrr {
    uuid_t uuid;
    u8 ip[4];
    u16 port;
    u16 begin[2];
    u16 cost[2];
    u16 stay[2];
};

struct rrhdr {
    struct sphdr sp_hdr;
    struct rrr rt[0];
};

static inline struct rrr *__rt_cur(struct rrhdr *rr_hdr) {
    struct sphdr *sp_hdr = &rr_hdr->sp_hdr;
    BUG_ON(sp_hdr->ttl < 1);
    return &rr_hdr->rt[sp_hdr->ttl - 1];
}

static inline struct rrr *rt_cur(char *ubuf) {
    struct rrhdr *rr_hdr = (struct rrhdr *)get_sphdr(ubuf);
    return __rt_cur(rr_hdr);
}

static inline struct rrr *__rt_prev(struct rrhdr *rr_hdr) {
    struct sphdr *sp_hdr = &rr_hdr->sp_hdr;
    BUG_ON(sp_hdr->ttl < 2);
    return &rr_hdr->rt[sp_hdr->ttl - 2];
}

static inline struct rrr *rt_prev(char *ubuf) {
    struct rrhdr *rr_hdr = (struct rrhdr *)get_sphdr(ubuf);
    return __rt_prev(rr_hdr);
}

static inline char *__rt_append(char *hdr, struct rrr *r) {
    u32 hlen = xubuflen(hdr);
    char *nhdr = xallocubuf(hlen + sizeof(*r));
    memcpy(nhdr, hdr, hlen);
    xfreeubuf(hdr);
    ((struct sphdr *)nhdr)->ttl++;
    *__rt_cur((struct rrhdr *)nhdr) = *r;
    return nhdr;
}

static inline void rt_append(char *ubuf, struct rrr *r) {
    int rc;
    struct xcmsg ent = { 0, 0 };

    rc = xmsgctl(ubuf, XMSG_RMCMSG, &ent);
    BUG_ON(rc || !ent.outofband);
    ent.outofband = __rt_append(ent.outofband, r);
    BUG_ON((rc = xmsgctl(ubuf, XMSG_ADDCMSG, &ent)));
}



#endif
