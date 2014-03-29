#ifndef _HPIO_TRANSPORT_
#define _HPIO_TRANSPORT_

#include "os/memory.h"
#include "hash/crc.h"

#define CHANNEL_LINGER 1
#define CHANNEL_SNDBUF 2
#define CHANNEL_RCVBUF 3
#define CHANNEL_SNDTIMEO 4
#define CHANNEL_RCVTIMEO 5

/*
  The transport protocol header is 10 bytes long and looks like this:

  +--------+------------+------------+
  | 0xffff | 0xffffffff | 0xffffffff |
  +--------+------------+------------+
  |  crc16 |    ctlsz   | payloadlen |
  +--------+------------+------------+
*/

#define PIO_MSG -1

struct channel_msghdr {
    uint16_t checksum;
    uint32_t payload_sz;
    uint32_t control_sz;
};

struct channel_msg {
    char *payload;
    char *control;
};

static inline struct channel_msg *channel_allocmsg(
        uint32_t payload_sz, uint32_t control_sz) {
    struct channel_msghdr *hdr;
    struct channel_msg *msg;
    char *chunk = (char *)mem_zalloc(sizeof(*msg) + sizeof(*hdr) + payload_sz + control_sz);
    if (!chunk)
	return NULL;
    hdr = (struct channel_msghdr *)(chunk + sizeof(*msg));
    hdr->payload_sz = payload_sz;
    hdr->control_sz = control_sz;
    hdr->checksum = crc16(&hdr->payload_sz, 16);
    msg = (struct channel_msg *)chunk;
    msg->payload = chunk + sizeof(*msg) + sizeof(*hdr);
    msg->control = msg->payload + payload_sz;
    return msg;
}

static inline void channel_freemsg(struct channel_msg *msg) {
    mem_free(msg);
}

int channel_bind(const char *addr);
int channel_connect(const char *addr);
void channel_close(int cid);
int channel_send(int cid, const struct channel_msg *msg);
int channel_recv(int cid, struct channel_msg **msg);
int channel_setopt(int cid, int opt, const void *val, int len);
int channel_getopt(int cid, int opt, void *val, int *len);


#endif
