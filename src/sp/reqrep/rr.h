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
 * | sphdr |  rt_entry[]  |  uhdr |  ubuf  |
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

static inline struct rrhdr *get_rrhdr(char *ubuf) {
	return (struct rrhdr *)get_sphdr(ubuf);
}

static inline struct rrhdr *new_rrhdr(struct rtentry *rt) {
	struct sphdr *sh = 0;
	struct rrhdr *pg = 0;

	pg = (struct rrhdr *)xallocubuf(sizeof(*pg) + sizeof(*rt));
	BUG_ON(!pg);
	sh = &pg->sh;
	sh->protocol = SP_REQREP;
	sh->version = SP_REQREP_VERSION;
	sh->timeout = 0;
	sh->sendstamp = 0;
	pg->go = 1;
	pg->ttl = 1;
	pg->end_ttl = 0;
	pg->rt[0] = *rt;
	return pg;
}

static inline struct rtentry *__rt_cur(struct rrhdr *pg) {
	BUG_ON(pg->ttl < 1);
	return &pg->rt[pg->ttl - 1];
}

static inline struct rtentry *rt_cur(char *ubuf) {
	struct rrhdr *pg = (struct rrhdr *)get_sphdr(ubuf);
	return __rt_cur(pg);
}

static inline struct rtentry *__rt_prev(struct rrhdr *pg) {
	BUG_ON(pg->ttl < 2);
	return &pg->rt[pg->ttl - 2];
}

static inline struct rtentry *rt_prev(char *ubuf) {
	struct rrhdr *pg = (struct rrhdr *)get_sphdr(ubuf);
	return __rt_prev(pg);
}

static inline char *__rt_append(char *hdr, struct rtentry *rt)
{
	u32 hlen = xubuflen(hdr);
	char *nhdr = xallocubuf(hlen + sizeof(*rt));
	memcpy(nhdr, hdr, hlen);
	xfreeubuf(hdr);
	((struct rrhdr *)nhdr)->ttl++;
	*__rt_cur((struct rrhdr *)nhdr) = *rt;
	return nhdr;
}

static inline void rt_append(char *ubuf, struct rtentry *rt)
{
	char *sh_ubuf = ubufctl_first(ubuf);

	ubufctl_rm(ubuf, sh_ubuf);
	sh_ubuf = __rt_append(sh_ubuf, rt);
	ubufctl_add(ubuf, sh_ubuf);
}

struct rrtgtd {
	struct tgtd tg;
	uuid_t uuid;
	struct skbuf_head snd;
};

static inline struct rrtgtd *get_rrtgtd(struct tgtd *tg) {
	return cont_of(tg, struct rrtgtd, tg);
}

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
