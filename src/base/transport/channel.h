#ifndef _HPIO_TRANSPORT_
#define _HPIO_TRANSPORT_

#include "os/memory.h"
#include "hash/crc.h"

#define TP_LINGER 1
#define TP_SNDBUF 2
#define TP_RCVBUF 3
#define TP_SNDTIMEO 4
#define TP_RCVTIMEO 5

/*
  The transport protocol header is 10 bytes long and looks like this:

  +--------+------------+------------+
  | 0xffff | 0xffffffff | 0xffffffff |
  +--------+------------+------------+
  |  crc16 |    ctlsz   | payloadlen |
  +--------+------------+------------+
*/

#define PIO_MSG -1

struct tp_msghdr {
    uint16_t checksum;
    uint32_t payload_sz;
    uint32_t control_sz;
};

struct tp_msg {
    char *payload;
    char *control;
};

static inline struct tp_msg *tp_allocmsg(uint32_t payload_sz,
    uint32_t control_sz) {
    struct tp_msghdr *hdr;
    struct tp_msg *msg;
    char *chunk = (char *)mem_zalloc(sizeof(*msg) + sizeof(*hdr)
        + payload_sz + control_sz);
    if (!chunk)
	return NULL;
    hdr = (struct tp_msghdr *)(chunk + sizeof(*msg));
    hdr->payload_sz = payload_sz;
    hdr->control_sz = control_sz;
    hdr->checksum = crc16(&hdr->payload_sz, 16);
    msg = (struct tp_msg *)chunk;
    msg->payload = chunk + sizeof(*msg) + sizeof(*hdr);
    msg->control = msg->payload + payload_sz;
    return msg;
}

static inline void tp_freemsg(struct tp_msg *msg) {
    mem_free(msg);
}

struct transport {
    const char *proto;
};

struct transport *tp_bind(const char *addr);
struct transport *tp_connect(const char *addr);
void tp_close(struct transport *tp);
int tp_send(struct transport *tp, const struct tp_msg *msg);
int tp_recv(struct transport *tp, struct tp_msg **msg);
int tp_setopt(struct transport *tp, int opt, const void *val, int len);
int tp_getopt(struct transport *tp, int opt, void *val, int *len);


#endif
