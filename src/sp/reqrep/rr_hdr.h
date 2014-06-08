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
 * +-------+-------------+-------+--------+
 * | sphdr |  rtentry[]  |  uhdr |  ubuf  |
 * +-------+-------------+-------+--------+
 */

struct rtentry {
    uuid_t uuid;
    u8 ip[4];
    u16 port;
    u16 begin[2];
    u16 cost[2];
    u16 stay[2];
};

struct rrhdr {
    struct sphdr sh;
    u16 ttl:4;
    u16 end_ttl:4;
    u16 go:1;
    struct rtentry rt[0];
};

static inline struct rrhdr *rqhdr_first(struct rtentry *r) {
    struct sphdr *sp_hdr = 0;
    struct rrhdr *rr_hdr = 0;

    rr_hdr = (struct rrhdr *)xallocubuf(sizeof(*rr_hdr) + sizeof(*r));
    BUG_ON(!rr_hdr);
    sp_hdr = &rr_hdr->sh;
    sp_hdr->protocol = SP_REQREP;
    sp_hdr->version = SP_REQREP_VERSION;
    sp_hdr->timeout = 0;
    sp_hdr->sendstamp = 0;
    rr_hdr->go = 1;
    rr_hdr->ttl = 1;
    rr_hdr->end_ttl = 0;
    rr_hdr->rt[0] = *r;
    return rr_hdr;
}

static inline struct rrhdr *get_rrhdr(char *ubuf) {
    return (struct rrhdr *)get_sphdr(ubuf);
}

static inline struct rtentry *__rt_cur(struct rrhdr *rr_hdr) {
    BUG_ON(rr_hdr->ttl < 1);
    return &rr_hdr->rt[rr_hdr->ttl - 1];
}

static inline struct rtentry *rt_cur(char *ubuf) {
    struct rrhdr *rr_hdr = (struct rrhdr *)get_sphdr(ubuf);
    return __rt_cur(rr_hdr);
}

static inline struct rtentry *__rt_prev(struct rrhdr *rr_hdr) {
    BUG_ON(rr_hdr->ttl < 2);
    return &rr_hdr->rt[rr_hdr->ttl - 2];
}

static inline struct rtentry *rt_prev(char *ubuf) {
    struct rrhdr *rr_hdr = (struct rrhdr *)get_sphdr(ubuf);
    return __rt_prev(rr_hdr);
}

static inline char *__rt_append(char *hdr, struct rtentry *r)
{
    u32 hlen = xubuflen(hdr);
    char *nhdr = xallocubuf(hlen + sizeof(*r));
    memcpy(nhdr, hdr, hlen);
    xfreeubuf(hdr);
    ((struct rrhdr *)nhdr)->ttl++;
    *__rt_cur((struct rrhdr *)nhdr) = *r;
    return nhdr;
}

static inline void rt_append(char *ubuf, struct rtentry *r)
{
    int rc;
    struct xcmsg ent = { 0, 0 };

    rc = xmsgctl(ubuf, XMSG_RMCMSG, &ent);
    BUG_ON(rc || !ent.outofband);
    ent.outofband = __rt_append(ent.outofband, r);
    BUG_ON((rc = xmsgctl(ubuf, XMSG_ADDCMSG, &ent)));
}



struct rr_tgtd {
    struct tgtd base;
    uuid_t uuid;
    struct list_head snd_cache;
};

static inline void __tgtd_try_enable_out(struct tgtd *tg)
{
    struct epbase *ep = tg->owner;

    if (!(tg->ent.events & XPOLLOUT)) {
        sg_update_tg(tg, tg->ent.events | XPOLLOUT);
        DEBUG_OFF("ep %d socket %d enable pollout", ep->eid, tg->fd);
    }
}

static inline void __tgtd_try_disable_out(struct tgtd *tg)
{
    struct epbase *ep = tg->owner;

    if ((tg->ent.events & XPOLLOUT)) {
	sg_update_tg(tg, tg->ent.events & ~XPOLLOUT);
        DEBUG_OFF("ep %d socket %d enable pollout", ep->eid, tg->fd);
    }
}



#endif
