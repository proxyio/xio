#ifndef _HPIO_HDR_
#define _HPIO_HDR_

#include <errno.h>
#include <uuid/uuid.h>
#include <ds/list.h>
#include <os/timesz.h>


struct ep_rt {
    uuid_t uuid;
    u8 ip[4];
    u16 port;
    u16 begin[2];
    u16 cost[2];
    u16 stay[2];
};

struct ep_hdr {
    u8 version;
    u16 ttl:4;
    u16 end_ttl:4;
    u16 go:1;
    u32 size;
    u16 timeout;
    i64 sendstamp;
    u16 checksum;
    union {
	u64 __align[2];
	struct list_head link;
    } u;
    struct ep_rt rt[0];
};

#define list_for_each_ep_hdr(h, nh, head)				\
    list_for_each_entry_safe(h, nh, head, struct ep_hdr, u.link)

static inline u32 hdr_size(struct ep_hdr *h) {
    u32 ttl = h->go ? h->ttl : h->end_ttl;
    return sizeof(*h) + ttl * sizeof(struct ep_rt);
}

static inline int ep_hdr_timeout(struct ep_hdr *h) {
    return h->timeout && (h->sendstamp + h->timeout < rt_mstime());
}

int ep_hdr_validate(struct ep_hdr *h);
void ep_hdr_gensum(struct ep_hdr *h);

#define rt_cur(h) (&(h)->rt[(h)->ttl - 1])
#define rt_prev(h) (&(h)->rt[(h)->ttl - 2])

static inline void rt_go_cost(struct ep_hdr *h) {
    struct ep_rt *cr = rt_cur(h);
    cr->cost[0] = (u16)(rt_mstime() - h->sendstamp - cr->begin[0]);
}

static inline void rt_back_cost(struct ep_hdr *h) {
    struct ep_rt *cr = rt_cur(h);
    cr->cost[1] = (u16)(rt_mstime() - h->sendstamp - cr->begin[1]);
}

struct ep_hdr *rt_append_and_go(struct ep_hdr *h, struct ep_rt *r);
void rt_shrink_and_back(struct ep_hdr *h);

#endif
