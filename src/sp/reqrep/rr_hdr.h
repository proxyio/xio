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

struct rr_package {
    struct sphdr sh;
    u16 ttl:4;
    u16 end_ttl:4;
    u16 go:1;
    struct rtentry rt[0];
};

static inline struct rr_package *rqhdr_first(struct rtentry *r) {
    struct sphdr *sp_hdr = 0;
    struct rr_package *pg = 0;

    pg = (struct rr_package *)xallocubuf(sizeof(*pg) + sizeof(*r));
    BUG_ON(!pg);
    sp_hdr = &pg->sh;
    sp_hdr->protocol = SP_REQREP;
    sp_hdr->version = SP_REQREP_VERSION;
    sp_hdr->timeout = 0;
    sp_hdr->sendstamp = 0;
    pg->go = 1;
    pg->ttl = 1;
    pg->end_ttl = 0;
    pg->rt[0] = *r;
    return pg;
}

static inline struct rr_package *get_rr_package(char *ubuf) {
    return (struct rr_package *)get_sphdr(ubuf);
}

static inline struct rtentry *__rt_cur(struct rr_package *pg) {
    BUG_ON(pg->ttl < 1);
    return &pg->rt[pg->ttl - 1];
}

static inline struct rtentry *rt_cur(char *ubuf) {
    struct rr_package *pg = (struct rr_package *)get_sphdr(ubuf);
    return __rt_cur(pg);
}

static inline struct rtentry *__rt_prev(struct rr_package *pg) {
    BUG_ON(pg->ttl < 2);
    return &pg->rt[pg->ttl - 2];
}

static inline struct rtentry *rt_prev(char *ubuf) {
    struct rr_package *pg = (struct rr_package *)get_sphdr(ubuf);
    return __rt_prev(pg);
}

static inline char *__rt_append(char *hdr, struct rtentry *r)
{
    u32 hlen = xubuflen(hdr);
    char *nhdr = xallocubuf(hlen + sizeof(*r));
    memcpy(nhdr, hdr, hlen);
    xfreeubuf(hdr);
    ((struct rr_package *)nhdr)->ttl++;
    *__rt_cur((struct rr_package *)nhdr) = *r;
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
