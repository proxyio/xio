#ifndef _XIO_EPHDR_
#define _XIO_EPHDR_

#include <errno.h>
#include <uuid/uuid.h>
#include <ds/list.h>
#include <os/timesz.h>
#include <hash/crc.h>

/* The ephdr looks like this:
 * +-------+---------+-------+----------------+
 * | ephdr |  epr[]  |  uhdr |  udata content |
 * +-------+---------+-------+----------------+
 */

struct epr {
    uuid_t uuid;
    u8 ip[4];
    u16 port;
    u16 begin[2];
    u16 cost[2];
    u16 stay[2];
};

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

struct uhdr {
    u16 ephdr_off;
};

static inline u32 ephdr_rtlen(struct ephdr *h) {
    u32 ttl = h->go ? h->ttl : h->end_ttl;
    return ttl * sizeof(struct epr);
}

/* The ep_buf length = ephdr_ctlen() + ephdr_dlen() */
static inline u32 ephdr_ctlen(struct ephdr *h) {
    return sizeof(*h) + ephdr_rtlen(h) + sizeof(struct uhdr);
}

static inline u32 ephdr_dlen(struct ephdr *h) {
    return h->size;
}

struct ephdr *udata2ephdr(void *udata) {
    struct uhdr *uh = (struct uhdr *)(udata - sizeof(*uh));
    return (struct ephdr *)(udata - uh->ephdr_off);
}

void *ephdr2udata(struct ephdr *h) {
    return ((void *)h) + ephdr_ctlen(h);
}

static inline int ephdr_timeout(struct ephdr *h) {
    return h->timeout && (h->sendstamp + h->timeout < rt_mstime());
}

static inline struct epr *rt_cur(struct ephdr *h) {
    return &h->rt[h->ttl - 1];
}

static inline struct epr *rt_prev(struct ephdr *h) {
    return &h->rt[h->ttl - 2];
}

#endif
