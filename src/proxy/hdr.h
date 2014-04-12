#ifndef _HPIO_HDR_
#define _HPIO_HDR_

#include <errno.h>
#include <uuid/uuid.h>
#include "ds/list.h"
#include "ep.h"

#define URLNAME_MAX 128

struct hgr {
    u32 type;
    uuid_t id;
    char group[URLNAME_MAX];
};

struct tr {
    uuid_t uuid;
    u8 ip[4];
    u16 port;
    u16 begin[2];
    u16 cost[2];
    u16 stay[2];
};

struct rdh {
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

struct gsm {
    char *payload;
    struct rdh *h;
    struct tr *r;
    struct list_head link;
};

static inline uint32_t tr_size(struct rdh *h) {
    uint32_t ttl = h->go ? h->ttl : h->end_ttl;
    return ttl * sizeof(struct tr);
}

static inline int gsm_timeout(struct gsm *s, int64_t now) {
    struct rdh *h = s->h;
    return h->timeout && (h->sendstamp + h->timeout < now);
}

int gsm_validate(struct gsm *s);
void gsm_gensum(struct gsm *s);


#define list_for_each_msg_safe(s, nx, head)			\
    list_for_each_entry_safe(s, nx, head, struct gsm, link)

#define tr_cur(s) (&(s)->r[(s)->h->ttl - 1])
#define tr_prev(s) (&(s)->r[(s)->h->ttl - 2])

static inline void tr_go_cost(struct gsm *s, int64_t now) {
    struct rdh *h = s->h;
    struct tr *r = tr_cur(s);
    r->stay[0] = (uint16_t)(now - h->sendstamp - r->begin[0]);
}

static inline void tr_back_cost(struct gsm *s, int64_t now) {
    struct rdh *h = s->h;
    struct tr *r = tr_cur(s);
    r->cost[1] = (uint16_t)(now - h->sendstamp - r->begin[1]);
}

int tr_append_and_go(struct gsm *s, struct tr *r, int64_t now);
void tr_shrink_and_back(struct gsm *s, int64_t now);

struct gsm *gsm_new(char *payload);
void gsm_free(struct gsm *s);



#endif
