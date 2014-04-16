#ifndef _HPIO_HDR_
#define _HPIO_HDR_

#include <errno.h>
#include <uuid/uuid.h>
#include "ds/list.h"

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
    u16 icmp:1;
    u32 size;
    u16 timeout;
    u16 checksum;
    u64 seqid;
    i64 sendstamp;
};

struct ep_msg {
    char *payload;
    struct ep_hdr *h;
    struct ep_rt *r;
    struct list_head link;
};

#define list_for_each_ep_msg(s, ns, head)			\
    list_for_each_entry_safe(s, ns, head, struct ep_msg, link)

static inline u32 rt_size(struct ep_hdr *h) {
    u32 ttl = h->go ? h->ttl : h->end_ttl;
    return ttl * sizeof(struct ep_rt);
}

static inline int ep_msg_timeout(struct ep_msg *s, i64 now) {
    struct ep_hdr *h = s->h;
    return h->timeout && (h->sendstamp + h->timeout < now);
}

int ep_msg_validate(struct ep_msg *s);
void ep_msg_gensum(struct ep_msg *s);


#define list_for_each_msg_safe(s, nx, head)			\
    list_for_each_entry_safe(s, nx, head, struct ep_msg, link)

#define rt_cur(s) (&(s)->r[(s)->h->ttl - 1])
#define rt_prev(s) (&(s)->r[(s)->h->ttl - 2])

static inline void rt_go_cost(struct ep_msg *s, i64 now) {
    struct ep_hdr *h = s->h;
    struct ep_rt *r = rt_cur(s);
    r->cost[0] = (u16)(now - h->sendstamp - r->begin[0]);
}

static inline void rt_back_cost(struct ep_msg *s, i64 now) {
    struct ep_hdr *h = s->h;
    struct ep_rt *r = rt_cur(s);
    r->cost[1] = (u16)(now - h->sendstamp - r->begin[1]);
}

int rt_append_and_go(struct ep_msg *s, struct ep_rt *r, i64 now);
void rt_shrink_and_back(struct ep_msg *s, i64 now);

struct ep_msg *ep_msg_new(char *payload);
void ep_msg_init(struct ep_msg *s, char *payload);
void ep_msg_free(struct ep_msg *s);



#endif
