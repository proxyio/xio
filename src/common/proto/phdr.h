#ifndef _HPIO_HDR_
#define _HPIO_HDR_

#include <errno.h>
#include <uuid/uuid.h>
#include "ds/list.h"
#include "hash/crc.h"
#include "os/memory.h"

struct pio_rt {
    uuid_t uuid;
    uint8_t ip[4];
    uint16_t port;
    uint16_t cost;
    uint16_t stay;
    uint32_t begin;
};
#define PIORTLEN sizeof(struct pio_rt)

struct pio_hdr {
    uint8_t version;
    uint8_t ttl:4;
    uint8_t end_ttl:4;
    uint32_t size;
    uint16_t go:1;
    uint16_t flags:15;
    uint16_t hdrcheck;
    uint64_t seqid;
    int64_t sendstamp;
};
#define PIOHDRLEN sizeof(struct pio_hdr)

static inline uint32_t pio_rt_size(const struct pio_hdr *h) {
    uint32_t ttl = h->go ? h->ttl : h->end_ttl;
    return ttl * PIORTLEN;
}

static inline uint32_t pio_pkg_size(const struct pio_hdr *h) {
    return PIOHDRLEN + h->size + pio_rt_size(h);
}

static inline int ph_validate(const struct pio_hdr *h) {
    struct pio_hdr copyheader = *h;
    int ok;
    copyheader.hdrcheck = 0;
    if (!(ok = (crc16((char *)&copyheader, PIOHDRLEN) == h->hdrcheck)))
	errno = EPROTO;
    return ok;
}

static inline void ph_makechksum(struct pio_hdr *hdr) {
    struct pio_hdr copyheader = *hdr;
    copyheader.hdrcheck = 0;
    hdr->hdrcheck = crc16((char *)&copyheader, PIOHDRLEN);
}


typedef struct pio_msg {
    struct pio_hdr hdr;
    char *data;
    struct pio_rt *rt;
    struct list_head node;
} pio_msg_t;
#define PIOMSGLEN sizeof(struct pio_msg)

static inline uint32_t pio_msg_size(pio_msg_t *msg) {
    uint32_t ttl = msg->hdr.go ? msg->hdr.ttl : msg->hdr.end_ttl;
    return PIOMSGLEN + msg->hdr.size + ttl * PIORTLEN;
}

#define pio_msg_currt(msg) (&(msg)->rt[(msg)->hdr.ttl - 1])

static inline int pio_rt_append(pio_msg_t *msg, struct pio_rt *rt) {
    struct pio_rt *nrt;
    uint32_t new_sz = pio_rt_size(&msg->hdr) + PIORTLEN;
    if (!(nrt = (struct pio_rt *)mem_realloc(msg->rt, new_sz)))
	return false;
    msg->hdr.ttl++;
    msg->rt = nrt;
    memcpy(pio_msg_currt(msg), rt, PIORTLEN);
    ph_makechksum(&msg->hdr);
    return true;
}

static inline void pio_msg_free_data_and_rt(pio_msg_t *msg) {
    mem_free(msg->data, msg->hdr.size);
    mem_free(msg->rt, pio_rt_size(&msg->hdr));
}



#define PROXYNAME_MAX 128

enum {
    PIO_RCVER = 1,
    PIO_SNDER,
};

typedef struct pio_rgh {
    uint32_t type;
    uuid_t id;
    char proxyname[PROXYNAME_MAX];
} pio_rgh_t;
#define PIORGHLEN sizeof(struct pio_rgh)



#endif
