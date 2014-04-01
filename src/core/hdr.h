#ifndef _HPIO_HDR_
#define _HPIO_HDR_

#include <errno.h>
#include <uuid/uuid.h>
#include "ds/list.h"
#include "hash/crc.h"
#include "os/alloc.h"

typedef struct pio_rt {
    uuid_t uuid;
    uint8_t ip[4];
    uint16_t port;
    uint16_t begin[2];
    uint16_t cost[2];
    uint16_t stay[2];
} pio_rt_t;
#define PIORTLEN sizeof(pio_rt_t)

typedef struct pio_hdr {
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
} pio_hdr_t;
#define PIOHDRLEN sizeof(pio_hdr_t)

static inline uint32_t rt_size(const pio_hdr_t *h) {
    uint32_t ttl = h->go ? h->ttl : h->end_ttl;
    return ttl * PIORTLEN;
}

static inline uint32_t raw_pkg_size(const pio_hdr_t *h) {
    return PIOHDRLEN + h->size + rt_size(h);
}

static inline int ph_timeout(const pio_hdr_t *h, int64_t now) {
    return h->timeout && (h->sendstamp + h->timeout < now);
}

static inline int ph_validate(const pio_hdr_t *h) {
    pio_hdr_t copyheader = *h;
    int ok;
    copyheader.checksum = 0;
    if (!(ok = (crc16((char *)&copyheader, PIOHDRLEN) == h->checksum)))
	errno = EPROTO;
    return ok;
}

static inline void ph_makechksum(pio_hdr_t *hdr) {
    pio_hdr_t copyheader = *hdr;
    copyheader.checksum = 0;
    hdr->checksum = crc16((char *)&copyheader, PIOHDRLEN);
}


typedef struct {
    pio_hdr_t hdr;
    char *data;
    pio_rt_t *rt;
    struct list_head mq_link;
} pio_msg_t;
#define PIOMSGLEN sizeof(pio_msg_t)

#define list_for_each_msg_safe(pos, tmp, head)				\
    list_for_each_entry_safe(pos, tmp, head, pio_msg_t, mq_link)

#define cur_rt(msg) (&(msg)->rt[(msg)->hdr.ttl - 1])
#define prev_rt(msg) (&(msg)->rt[(msg)->hdr.ttl - 2])

static inline void rt_go_cost(pio_msg_t *msg, int64_t now) {
    pio_hdr_t *h = &msg->hdr;
    pio_rt_t *rt = cur_rt(msg);
    rt->stay[0] = (uint16_t)(now - h->sendstamp - rt->begin[0]);
}

static inline void rt_back_cost(pio_msg_t *msg, int64_t now) {
    pio_hdr_t *h = &msg->hdr;
    pio_rt_t *rt = cur_rt(msg);
    rt->cost[1] = (uint16_t)(now - h->sendstamp - rt->begin[1]);
}

static inline int rt_append_and_go(pio_msg_t *msg, pio_rt_t *rt,
				   int64_t now) {
    pio_rt_t *crt;
    pio_hdr_t *h = &msg->hdr;
    uint32_t new_sz = rt_size(h) + PIORTLEN;
    if (!(crt = (pio_rt_t *)mem_realloc(msg->rt, new_sz)))
	return false;
    msg->rt = crt;
    crt = cur_rt(msg);
    crt->stay[0] = (uint16_t)(now - h->sendstamp - crt->begin[0]);
    h->ttl++;
    rt->begin[0] = (uint16_t)(now - h->sendstamp);
    memcpy(cur_rt(msg), rt, PIORTLEN);
    ph_makechksum(h);
    return true;
}

static inline void rt_shrink_and_back(pio_msg_t *msg, int64_t now) {
    pio_rt_t *crt = cur_rt(msg);
    pio_hdr_t *h = &msg->hdr;
    crt->stay[1] = (uint16_t)(now - h->sendstamp - crt->begin[1] - crt->cost[1]);
    msg->hdr.ttl--;
    ph_makechksum(&msg->hdr);
    crt = cur_rt(msg);
    crt->begin[1] = (uint16_t)(now - msg->hdr.sendstamp);
}

static inline void free_msg_data_and_rt(pio_msg_t *msg) {
    mem_free(msg->data, msg->hdr.size);
    mem_free(msg->rt, rt_size(&msg->hdr));
}



#define PROXYNAME_MAX 128

enum {
    PIO_RCVER = 0x01,
    PIO_SNDER = 0x02,
};

typedef struct pio_rgh {
    uint32_t type;
    uuid_t id;
    char proxyname[PROXYNAME_MAX];
} pio_rgh_t;
#define PIORGHLEN sizeof(struct pio_rgh)



#endif
