#ifndef _HPIO_HDR_
#define _HPIO_HDR_

#include <errno.h>
#include <uuid/uuid.h>
#include "channel/channel.h"
#include "ds/list.h"
#include "hash/crc.h"

#define PROXYNAME_MAX 128
#define PIO_RCVER  1
#define PIO_SNDER  2

struct hgr {
    uint32_t type;
    uuid_t id;
    char proxyname[PROXYNAME_MAX];
};

struct tr {
    uuid_t uuid;
    uint8_t ip[4];
    uint16_t port;
    uint16_t begin[2];
    uint16_t cost[2];
    uint16_t stay[2];
};

struct rdh {
    uint8_t version;
    uint16_t ttl:4;
    uint16_t end_ttl:4;
    uint16_t go:1;
    uint16_t icmp:1;
    uint32_t size;
    uint16_t timeout;
    uint16_t checksum;
    uint64_t seqid;
    int64_t sendstamp;
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

static inline uint32_t gsm_size(struct rdh *h) {
    return sizeof(struct rdh) + h->size + tr_size(h);
}

static inline int gsm_timeout(struct rdh *h, int64_t now) {
    return h->timeout && (h->sendstamp + h->timeout < now);
}

int gsm_validate(struct rdh *h);
void gsm_gensum(struct rdh *h);


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

static inline struct gsm *gsm_new(char *payload) {
    struct gsm *s = (struct gsm *)mem_zalloc(sizeof(*s));
    if (s) {
	INIT_LIST_HEAD(&s->link);
	s->payload = payload;
	s->h = (struct rdh *)payload;
	s->r = payload + s->h->size;
    }
    return s;
}

static inline void gsm_free(struct gsm *s) {
    channel_freemsg(s->payload);
    mem_free(s, sizeof(struct gsm));
}


#endif
