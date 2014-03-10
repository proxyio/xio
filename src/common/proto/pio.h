#ifndef _H_PIOHDR_
#define _H_PIOHDR_

#include <uuid/uuid.h>
#include "ds/list.h"
#include "hash/crc.h"
#include "os/malloc.h"

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

static inline int ph_validate(struct pio_hdr *hdr) {
    struct pio_hdr copyheader = *hdr;

    copyheader.hdrcheck = 0;
    return (crc16((char *)&copyheader, PIOHDRLEN) == hdr->hdrcheck);
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

static inline pio_msg_t *pio_msg_clone_reserve(pio_msg_t *msg, uint32_t reserve) {
    pio_msg_t *msg2 = (pio_msg_t *)mem_zalloc(pio_msg_size(msg) + reserve);
    if (msg2) {
	memcpy(&msg2->hdr, &msg->hdr, PIOHDRLEN);
	msg2->data = (char *)msg2 + PIOMSGLEN;
	msg2->rt = (struct pio_rt *)(msg2->data + msg2->hdr.size);
    }
    return msg2;
}

static inline pio_msg_t *pio_msg_clone(pio_msg_t *msg) {
    return pio_msg_clone_reserve(msg, 0);
}

#define pio_cur_rt(msg) ((msg)->rt[(msg)->hdr.ttl - 1])

#define pio_msg_appendrt(msg, rt) do {				\
	msg->hdr.ttl++;						\
	memcpy(&msg->rt[msg->hdr.ttl - 1], rt, PIORTLEN);	\
    } while (0)

#define pio_msg_shrinkrt(msg) do {		\
	msg->hdr.ttl--;				\
    } while (0)

#define pio_msg_free(msg) do {			\
	mem_free(msg, pio_msg_size(msg));	\
    } while (0)



#define GRPNAME_MAX 128

enum {
    PIO_RCVER = 1,
    PIO_SNDER,
};

typedef struct pio_rgh {
    uint32_t type;
    uuid_t id;
    char grpname[GRPNAME_MAX];
} pio_rgh_t;
#define PIORGHLEN sizeof(struct pio_rgh)



#endif
